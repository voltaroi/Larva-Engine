#pragma once
#include <string>
#include <winsock2.h>
#include <iostream>
#include <thread>

class Client {
public:
    Client();
    ~Client();

    bool connectToServer(const std::string& host, int port);
    void sendMessage(const std::string& msg);
    void disconnect();

    void receiveLoop();

private:
    SOCKET clientSocket = INVALID_SOCKET;
    bool running = false;
};
