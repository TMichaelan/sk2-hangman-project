#pragma once

#include <iostream>
#include <algorithm>
#include <cstring>
#include <random>
#include <vector>
#include "threads.h"
#include "prefixes.h"

#define PLAYER_LIVES 6
#define MAX_NUMBER_OF_PLAYERS 5


using namespace std;



class Player {
    public:
        int fd;
        string username;
        int score;
        int lives;
        
    public:
        Player() {};

        //construct player
        Player(const int fd, const string& username) {
            this->fd        = fd;
            this->username  = username;
            this->score    = 0;
            this->lives     = PLAYER_LIVES;
        }
};


class Room {
public:
    bool isStarted;
    string name;
    string word;
    string predicted;
    vector<Player> players;
    string WHITESPACE = " \n\r\t\f\v";

    Room (const string& name) {
        this->name = name;
        this->isStarted = false;
        this->word = getRandomWord("answers.txt");

        size_t end = this->word.find_last_not_of(WHITESPACE);

        this->word = this->word.substr(0, end + 1);
        this->predicted = "";

        //add to the variable predicted * for display by the client
        for (size_t i = 0; i < this->word.length(); ++i) {
            predicted += "*";
        }
    }

    string getRandomWord(string filename) {
        ifstream readFile(filename);
        if (!readFile.is_open()) {
            error(1, 0, "Could not find file with words!");
        }

        int lines = count(istreambuf_iterator<char>(readFile),
                    istreambuf_iterator<char>(), '\n');

        mt19937 gen((random_device()()));
        uniform_int_distribution<uint8_t> dist(0, lines-1);
        int randomLine = dist(gen);


        readFile.clear();
        //Sets the position of the next character
        readFile.seekg(0);
        
        string word;
        for (int i = 0; i <= randomLine; ++i) {
            getline(readFile, word);
        }
        readFile.close();
        
        return word;
    }
};

class Data {
public:
    vector<Player> players;
    vector<Room> rooms;

    Data() {}

    // handle player
    void addPlayer(const Player& player) {
        players.emplace_back(player);
    }

    bool findUsername(const string& username) {
        for (Player player : players) {
            if (player.username == username) {
                return true;
            }
        }
        return false;
    }

    string joinPlayer(string data) {
        size_t separatorIdx = data.find(DATA_SEPARATOR);
        if (separatorIdx == string::npos) {
            return FAILURE_CODE + string(DATA_SEPARATOR) + "Server error!";
        }

        string username = data.substr(0, separatorIdx);
        int clientFd = atoi(data.substr(separatorIdx+1).c_str());
    
        bool playerIdx = findUsername(username);
        if (!playerIdx) {
            addPlayer(Player(clientFd, username));
            cout << "Added player: " << clientFd << " with username: " << username  << endl;
            return string(CREATE_USERNAME_PREFIX) + string(DATA_SEPARATOR) + SUCCESS_CODE + string(DATA_SEPARATOR) + "Username doesn't exist";
        }
        return string(CREATE_USERNAME_PREFIX) + string(DATA_SEPARATOR) + FAILURE_CODE + string(DATA_SEPARATOR) + "Username already exists!";
    }

    int findPlayer(int& clientFd) {
        for (size_t i = 0; i < players.size(); ++i) {
            if (players[i].fd == clientFd) {
                return i;
            }
        }
        return -1;
    }

    bool deletePlayer(int& clientFd) {
        int indexToRemove = findPlayer(clientFd);
        if (indexToRemove != -1) {
            players.erase(players.begin()+indexToRemove);
            cout << "Deleted " << clientFd << endl;
            return true;
        }
        return false;
    }

    // handle room
    string sendRooms(string data) {
        string listOfRooms = string(SEND_ROOMS_PREFIX) + string(DATA_SEPARATOR) + string(SUCCESS_CODE);

        for (Room room : rooms) {
            listOfRooms += string(DATA_SEPARATOR) + room.name + string(DATA_SEPARATOR) + to_string(room.players.size());
        }

        return listOfRooms;
    }

    void addRoom(const Room& room) {
        //add room to vector "rooms"
        rooms.emplace_back(room);
    }

    int findRoom(const string& roomName) {
        for (size_t i = 0; i < rooms.size(); ++i) {
            if (rooms[i].name == roomName) {
                return i;
            }
        }
        return -1;
    }

    bool addPlayerToRoom(int clientFd, const string& roomName) {
        int roomIdx   = findRoom(roomName);
        int playerIdx = findPlayer(clientFd);
        if (roomIdx != -1 && playerIdx != -1) {
            //add player to room
            rooms[roomIdx].players.emplace_back(players[playerIdx]);
            return true;
        }
        return false;
    }

    int findPlayerInRoom(int clientFd, int roomIdx) {
        for (size_t i = 0; i < rooms[roomIdx].players.size(); ++i) {
            if (rooms[roomIdx].players[i].fd == clientFd) {
                return i;
            }
        }
        return -1;
    }

    bool deletePlayerFromRoom(int clientFd, const string& roomName) {
        int roomIdx = findRoom(roomName);
        if (roomIdx != -1) {
            int playerIdx = findPlayerInRoom(clientFd, roomIdx);
            rooms[roomIdx].players.erase(rooms[roomIdx].players.begin()+playerIdx);
            return true;
        }
        else {
            return false;
        }
    }

    string createRoom(string data) {
        size_t separatorIdx = data.find(DATA_SEPARATOR);
        if (separatorIdx == string::npos) {
            return FAILURE_CODE + string(DATA_SEPARATOR) + "Server error!";
        }

        string roomName = data.substr(0, separatorIdx);
        int clientFd = atoi(data.substr(separatorIdx+1).c_str());

        int roomIdx = findRoom(roomName);
        if (roomIdx == -1) {
            addRoom(Room(roomName));
            addPlayerToRoom(clientFd, roomName);
            
            return string(CREATE_ROOM_PREFIX) + string(DATA_SEPARATOR) + SUCCESS_CODE + string(DATA_SEPARATOR) + roomName;
        }
        return string(CREATE_ROOM_PREFIX) + string(DATA_SEPARATOR) + FAILURE_CODE + string(DATA_SEPARATOR) + "Room already exists!";
    }

    string chooseRoom(string data) {
        size_t separatorIdx = data.find(DATA_SEPARATOR);
        if (separatorIdx == string::npos) {
            return FAILURE_CODE + string(DATA_SEPARATOR) + "Server error!";
        }

        string roomName = data.substr(0, separatorIdx);
        int clientFd = atoi(data.substr(separatorIdx+1).c_str());

        int roomIdx = findRoom(roomName);
        if (roomIdx != -1) {
            if (rooms[roomIdx].isStarted == false && rooms[roomIdx].players.size() < MAX_NUMBER_OF_PLAYERS) {
                addPlayerToRoom(clientFd, roomName);
                return string(CHOOSE_ROOM_PREFIX) + string(DATA_SEPARATOR) + SUCCESS_CODE + string(DATA_SEPARATOR) + roomName;
            }
        }
        return string(CHOOSE_ROOM_PREFIX) + string(DATA_SEPARATOR) + FAILURE_CODE + string(DATA_SEPARATOR) + "Server error: Game has started or too many players!";
    }

    bool deleteRoom(const string& roomName) {
        int indexToRemove = findRoom(roomName);

        if (indexToRemove != -1) {
            rooms.erase(rooms.begin()+indexToRemove);
            cout << "Deleted Room" << roomName << endl;
            return true;
        }

        return false;
    }
};