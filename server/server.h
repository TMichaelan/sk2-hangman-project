#pragma once

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <error.h>
#include <errno.h>

#include "threads.h"
#include "prefixes.h"
#include "game.h"

using namespace std;


class Server
{
public:
    int serverFd;
    Data data;

    vector<thread> threads;
    Threads* threadEntryData;
    vector<Threads*> threadData;

    typedef string (Data::*function)(string);
    map<string, function> functionMap;

private:
    uint16_t port = 4000;

public:
    Server() {
        initServer();

        // map functions for entry thread
        functionMap[CREATE_USERNAME_PREFIX] = &Data::joinPlayer;
        functionMap[SEND_ROOMS_PREFIX]      = &Data::sendRooms;
        functionMap[CREATE_ROOM_PREFIX]     = &Data::createRoom;
        functionMap[CHOOSE_ROOM_PREFIX]     = &Data::chooseRoom;

        cout << "Server started at port " << port << endl;
    }

    ~Server() {
        if (threadEntryData != NULL) {
            threadEntryData->disconnected = true;
        }
        for (size_t i = 0; i < threadData.size(); ++i) {
            threadData[i]->disconnected = true;
        }
        for (size_t i = 0; i < threads.size(); ++i) {
            threads[i].join();
        }
        if (this->threadEntryData != NULL) {
            delete threadEntryData;
        }
        for (size_t i = 0; i < threadData.size(); ++i) {
            delete threadData[i];
        }
        close(serverFd);
        cout << "Server has been closed." << endl;
    }

    void errorHandler(int result, const char* errorMessage) {
        if (result == -1) {
            error(1, errno, "%s", errorMessage);
        }
    }

    void initServer() {
        errorHandler(this->serverFd = socket(AF_INET, SOCK_STREAM, 0), "Socket failed!");

        const int one = 1;
        errorHandler(setsockopt(this->serverFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)), "Setsockopt failed!");

        sockaddr_in serverAddr{
            .sin_family = AF_INET,
            .sin_port   = htons((short) this->port),
            .sin_addr   = {INADDR_ANY}
            // .sin_addr = inet_addr("172.17.112.1")
        };
        errorHandler(bind(this->serverFd, (sockaddr*)&serverAddr, sizeof(serverAddr)), "Bind failed!");
        errorHandler(listen(this->serverFd, QUEUE_SIZE), "Listen failed!");
 
    }

    int acceptConnection() {
        sockaddr_in clientAddr{};
        socklen_t clientAddrSize = sizeof(clientAddr);

        int clientFd;
        errorHandler(clientFd = accept(this->serverFd, (sockaddr*)&clientAddr, &clientAddrSize), "Accept failed!");
        return clientFd;
    }

    void writeMessage(Threads* thData, int clientFd, char* buff, string& message) {
        message += '\n';
        while(thData->saveToBuffer(buff, message) > 0) {
            if (write(clientFd, buff, BUFF_SIZE) == -1) {
                thData->removeDescriptor(clientFd);
                break;
            }
        }
    }

    void threadRoomFunction(Threads* thData) {
        int pollReady;
        char buff[BUFF_SIZE];
        string message, prefix;

        while (!thData->disconnected && thData->descriptors.size() > 0) {
            //wait for some event on a file descriptor
            pollReady = poll(&thData->descriptors[0], thData->descriptors.size(), thData->timeout);
            if (pollReady == -1) {
                error(0, errno, "Poll failed!");
                exit(SIGINT);
            }

            for (size_t i = 0; i < thData->descriptors.size() && pollReady > 0; ++i) {
                if (thData->descriptors[i].revents) {
                    auto clientFd = thData->descriptors[i].fd;
                    auto revents = thData->descriptors[i].revents;

                    if (revents & POLLIN) {
                        memset(buff, 0, BUFF_SIZE);
                        int bytesRead = read(clientFd, buff, BUFF_SIZE);
                        if (bytesRead <= 0) {
                            // delete player
                            thData->removeDescriptor(clientFd);
                            data.deletePlayer(clientFd);
                            data.deletePlayerFromRoom(clientFd, thData->name);
                            int roomIdx = data.findRoom(thData->name);
                            // if game didn't start, other player get information about it
                            if (!data.rooms[roomIdx].isStarted) {
                                // if all players left, the room will be deleted
                                if (data.rooms[roomIdx].players.size() < 1) {
                                    data.deleteRoom(data.rooms[roomIdx].name);
                                    thData->disconnected = true;
                                }
                                else {
                                    // update the number of players
                                    for (size_t j = 0; j < thData->descriptors.size(); ++j) {
                                        message = NUM_OF_PLAYERS_PREFIX + string(DATA_SEPARATOR) + SUCCESS_CODE + string(DATA_SEPARATOR)
                                                            + to_string(thData->descriptors.size());
                                        if (j == 0) {
                                            message += string(DATA_SEPARATOR) + "FIRST";
                                        }
                                        else {
                                            message += string(DATA_SEPARATOR) + "WAITING";
                                        }
                                        if (j == thData->descriptors.size()-1) {
                                            sleep(1);
                                        }
                                        writeMessage(thData, thData->descriptors[j].fd, buff, message);
                                    }
                                }
                            }
                            else {
                                // if number of players is less than 2 - end game
                                if (data.rooms[roomIdx].players.size() < 2) {
                                    // if game is end, room will deleted
                                    for (size_t j = 0; j < thData->descriptors.size(); ++j) {
                                        message = END_GAME_PREFIX + string(DATA_SEPARATOR) + SUCCESS_CODE + string(DATA_SEPARATOR) + "END";
                                        writeMessage(thData, thData->descriptors[j].fd, buff, message);
                                    }
                                    data.deleteRoom(data.rooms[roomIdx].name);
                                    thData->disconnected = true;
                                }
                            }
                            // update waiting for choose room information
                            for (size_t j = 0; j < threadEntryData->descriptors.size(); ++j) {
                                message = data.sendRooms("");
                                writeMessage(threadEntryData, threadEntryData->descriptors[j].fd, buff, message);
                            }
                        }
                        else {
                            thData->addBuffer(buff, i);
                            if (thData->readyRead(i)) {
                                message = thData->getMessage(i);
                                prefix  = thData->getPrefix(message);
                                message = thData->getArguments(message, i);
                                
                                if (prefix == START_GAME_PREFIX) {
                                    int index = data.findRoom(thData->name);
                                    data.rooms[index].isStarted = true;
                                    string buildMsg = string(START_GAME_PREFIX) + string(DATA_SEPARATOR) + SUCCESS_CODE 
                                                            + string(DATA_SEPARATOR) + data.rooms[index].predicted;
                                    for (size_t j = 0; j < data.rooms[index].players.size(); ++j) {
                                        buildMsg += string(DATA_SEPARATOR) + data.rooms[index].players[j].username;
                                    }

                                    // update waiting for choose room info
                                    for (size_t j = 0; j < thData->descriptors.size(); ++j) {
                                        message = buildMsg;
                                        writeMessage(thData, thData->descriptors[j].fd, buff, message);
                                    }
                                }
                                else if (prefix == GUESS_LETTER_PREFIX) {
                                    char letter = message[0];
                                    int roomIdx = data.findRoom(thData->name);

                                    // update word checking letter hits
                                    int score = 0;
                                    for (size_t j = 0; j < data.rooms[roomIdx].word.length(); ++j) {
                                        if (data.rooms[roomIdx].word[j] == letter && data.rooms[roomIdx].predicted[j] == '*') {
                                            data.rooms[roomIdx].predicted[j] = data.rooms[roomIdx].word[j];
                                            score += 1;
                                        }
                                    }
                                    
                                    // update guessing player state
                                    string buildMsg = string(GUESS_LETTER_PREFIX) + string(DATA_SEPARATOR) + SUCCESS_CODE
                                                            + string(DATA_SEPARATOR) + data.rooms[roomIdx].predicted + string(DATA_SEPARATOR);
                                    int playerIdx = data.findPlayerInRoom(clientFd, roomIdx);
                                    string hit = "MISS";
                                    if (score == 0) {
                                        if (data.rooms[roomIdx].players[playerIdx].lives > 0) {
                                            data.rooms[roomIdx].players[playerIdx].lives -= 1;
                                        }   
                                    }
                                    else {
                                        data.rooms[roomIdx].players[playerIdx].score += score;
                                        hit = letter;
                                    }
                                    // check if it's the end of the game
                                    string end = "NO";

                                    cout << data.rooms[roomIdx].word << endl;
                                    cout << data.rooms[roomIdx].word.length() << endl;
                                    cout << data.rooms[roomIdx].predicted <<endl;
                                    cout << data.rooms[roomIdx].predicted.length() <<endl;


                                    if (data.rooms[roomIdx].word == data.rooms[roomIdx].predicted) {
                                        end = "YES";
                                    }
                                    else {
                                        bool atLeastOneAlive = false;
                                        for (size_t j = 0; j < data.rooms[roomIdx].players.size(); ++j) {
                                            if (data.rooms[roomIdx].players[j].lives > 0) {
                                                atLeastOneAlive = true;
                                            }
                                        }
                                        if (!atLeastOneAlive) {
                                            end = "YES";
                                        }
                                    }
                                
                                    buildMsg += data.rooms[roomIdx].players[playerIdx].username + string(DATA_SEPARATOR) 
                                                + to_string(data.rooms[roomIdx].players[playerIdx].score) + string(DATA_SEPARATOR) 
                                                + to_string(data.rooms[roomIdx].players[playerIdx].lives) + string(DATA_SEPARATOR) 
                                                + hit + string(DATA_SEPARATOR) + end;

                                    // send information for all player
                                    for (size_t j = 0; j < thData->descriptors.size(); ++j) {
                                        message = buildMsg;
                                        writeMessage(thData, thData->descriptors[j].fd, buff, message);
                                    }
                                    // cleanup
                                    if (end == "YES") {
                                        data.deleteRoom(data.rooms[roomIdx].name);
                                        thData->disconnected = true;
                                        for (size_t j = 0; j < threadEntryData->descriptors.size(); ++j) {
                                            message = data.sendRooms("");
                                            writeMessage(threadEntryData, threadEntryData->descriptors[j].fd, buff, message);
                                        }
                                    }
                                }
                                else {
                                    message = FAILURE_CODE + string(DATA_SEPARATOR) + "Server error!";
                                    writeMessage(thData, clientFd, buff, message);
                                }
                            }
                        }
                        if (revents & ~POLLIN) {
                            thData->removeDescriptor(clientFd);
                        }
                    }
                    --pollReady;
                }
            }
        }
        // delete thread
        int index = -1;
        for (size_t i = 0; i < threadData.size(); ++i) {
            if (threadData[i]->name == thData->name) {
                index = i;
            }
        }
        if (index != -1) {
            threadData.erase(threadData.begin()+index);
        }
        delete thData;
    }

    void handleNewRooms(int clientFd, const string& roomName) {
        Threads* thData = new Threads(clientFd);
        thData->name = roomName;
        threadData.emplace_back(thData);
        threads.emplace_back(thread(&Server::threadRoomFunction, this, thData));
    }

    void addToRoomThread(int clientFd, const string& roomName) {
        for (size_t i = 0; i < threadData.size(); ++i) {
            if (threadData[i]->name == roomName) {
                threadData[i]->addNewDescriptor(clientFd);
                cout << "Player " << clientFd << " joined to the " << threadData[i]->name << " room" << endl;

                // give info to all room members
                char buff[BUFF_SIZE];
                string message;
                for (size_t j = 0; j < threadData[i]->descriptors.size(); ++j) {
                    message = NUM_OF_PLAYERS_PREFIX + string(DATA_SEPARATOR) + SUCCESS_CODE + string(DATA_SEPARATOR)
                                        + to_string(threadData[i]->descriptors.size());
                    if (j == 0) {
                        message += string(DATA_SEPARATOR) + "FIRST";
                    }
                    else {
                        message += string(DATA_SEPARATOR) + "WAITING";
                    }
                    if (j == threadData[i]->descriptors.size()-1) {
                        sleep(1);
                    }
                    writeMessage(threadData[i], threadData[i]->descriptors[j].fd, buff, message);
                }
            }
        }
    }

    void threadFunction(Threads* thData) {
        int pollReady;
        char buff[BUFF_SIZE];
        string message, prefix;

        while (!thData->disconnected) {
            pollReady = poll(&thData->descriptors[0], thData->descriptors.size(), thData->timeout);
            if (pollReady == -1) {
                error(0, errno, "Poll failed!");
                exit(SIGINT);
            }

            for (size_t i = 0; i < thData->descriptors.size() && pollReady > 0; ++i) {
                if (thData->descriptors[i].revents) {
                    auto clientFd = thData->descriptors[i].fd;
                    auto revents = thData->descriptors[i].revents;

                    if (revents & POLLIN) {
                        cout << "Reading data from client: " << clientFd << endl;
                        memset(buff, 0, BUFF_SIZE);
                        int bytesRead = read(clientFd, buff, BUFF_SIZE);
                        if (bytesRead <= 0) {
                            thData->removeDescriptor(clientFd);
                            data.deletePlayer(clientFd);
                        }
                        else {
                            thData->addBuffer(buff, i);
                            if (thData->readyRead(i)) {
                                message = thData->getMessage(i);
                                prefix  = thData->getPrefix(message);
                                message = thData->getArguments(message, i);

                                map<string, function>::iterator fun = functionMap.find(prefix);
                                if (fun == functionMap.end()) {
                                    message = FAILURE_CODE + string(DATA_SEPARATOR) + "Server error!";
                                }
                                else {
                                    message = (data.*fun->second)(message);
                                }
                                
                                // update rooms structure and information
                                if (prefix == CREATE_ROOM_PREFIX || prefix == CHOOSE_ROOM_PREFIX) {
                                    size_t sepIndex1 = message.find(DATA_SEPARATOR);
                                    size_t sepIndex2 = message.find_last_of(DATA_SEPARATOR);
                                    if (sepIndex1 == string::npos || sepIndex2 == string::npos) {
                                        cout << "Message error" << endl;
                                    }
                                    string code = message.substr(sepIndex1+1, sepIndex2-sepIndex1-1);
                                    string roomName = message.substr(sepIndex2+1, message.length());
                                    writeMessage(thData, clientFd, buff, message);
                                    
                                    if (code == SUCCESS_CODE) {
                                        // update descriptors
                                        if (prefix == CREATE_ROOM_PREFIX) {
                                            handleNewRooms(clientFd, roomName);
                                            thData->removeDescriptor(clientFd);
                                        }
                                        else if (prefix == CHOOSE_ROOM_PREFIX) {
                                            addToRoomThread(clientFd, roomName);
                                            thData->removeDescriptor(clientFd);
                                        }

                                        // update waiting for choose room information
                                        for (size_t j = 0; j < thData->descriptors.size(); ++j) {
                                            message = data.sendRooms("");
                                            writeMessage(thData, thData->descriptors[j].fd, buff, message);
                                        }
                                    }
                                }
                                // no actions needed
                                else {
                                    writeMessage(thData, clientFd, buff, message);
                                }
                                
                            }
                        }
                        if (revents & ~POLLIN) {
                            // cout << "Different revent captured: removing client descriptor..." << endl;
                            thData->removeDescriptor(clientFd);
                        }
                    }
                    --pollReady;
                }
            }
        }
    }

    void handleNewConnection(int clientFd) {
        //create new thread if there is not threads yet
        if (threads.size() == 0) {
            threadEntryData = new Threads(clientFd);
            threads.emplace_back(thread(&Server::threadFunction, this, threadEntryData));
        }
        else {
            threadEntryData->addNewDescriptor(clientFd);
        }
    }

    void loop() {
        while (true) {
            int clientFd = this->acceptConnection();

            this->handleNewConnection(clientFd);
        }
    }
};