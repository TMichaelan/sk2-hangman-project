#pragma once

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <poll.h>
#include <vector>

#include "prefixes.h"


using namespace std;

class Threads {
public:
    string name;
    vector<pollfd> descriptors;
    vector<string> message;

    bool disconnected;
    int timeout;

    Threads(int clientFd) {
        this->name = "";
        this->disconnected = false;
        this->timeout = 2500;
        this->addNewDescriptor(clientFd);
    }

    ~Threads() {
        for (size_t i = 0; i < descriptors.size(); ++i) {
            close(descriptors[i].fd);
            cout << "Descriptor " << descriptors[i].fd << " closed" << endl;
        }
    }

    // handle descriptors
    void addNewDescriptor(int clientFd) {
        pollfd descriptor;
        descriptor.fd = clientFd;
        descriptor.events = POLLIN;
        descriptors.emplace_back(descriptor);
        string msg = "";
        message.emplace_back(msg);
        cout << "Descriptor " << clientFd << " has been created" << endl;
    }

    
    int findDescriptor(int clientFd) {
        for (size_t i = 0; i < descriptors.size(); ++i) {
            if (descriptors[i].fd == clientFd) {
                return i;
            }
        }
        return -1;
    }

    bool removeDescriptor(int clientFd) {
        int indexToRemove = findDescriptor(clientFd);
        if (indexToRemove != -1) {
            descriptors.erase(descriptors.begin() + indexToRemove);
            return true;
        }
        return false;
    }

    //  handle messages
    void addBuffer(char* buff, int index) {
        message[index] += string(buff);
    }

    bool readyRead(int index) {
        return message[index].back() == DATA_END;
    }

    string getMessage(int index) {
        string result = message[index];
        message[index] = "";
        return result;
    }

    string getPrefix(const string& message) {
        size_t separatorIdx = message.find(DATA_SEPARATOR);
        if (separatorIdx == string::npos) {
            return "Could not get prefix";
        }
        return message.substr(0, separatorIdx);
    }

    string getArguments(string& message, int index) {
        message = message.substr(0, message.length()-1);
        size_t separatorIdx = message.find(DATA_SEPARATOR);
        if (separatorIdx == string::npos) {
            return "Could not get prefix";
        }

        string prefix = message.substr(0, separatorIdx);
        if (prefix == CREATE_USERNAME_PREFIX || prefix == SEND_ROOMS_PREFIX || prefix == CREATE_ROOM_PREFIX || prefix == CHOOSE_ROOM_PREFIX) {
            return message.substr(separatorIdx+1) + to_string(descriptors[index].fd);
        }

        return message.substr(separatorIdx+1);
    }

    int saveToBuffer(char* buff, string& message) {
        int bytes = min(BUFF_SIZE, (int)message.size());
        memset(buff, 0, BUFF_SIZE);
        sprintf(buff, "%s", message.substr(0, bytes).c_str());
        message = message.substr(bytes);
        return bytes;
    }
};