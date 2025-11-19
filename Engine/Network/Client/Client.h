#pragma once
#include <string>
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <functional>

class Client {
public:
    Client();
    ~Client();

    bool connectToServer(const std::string& host, int port);
    void sendMessage(const std::string& msg);
    void disconnect();

    void receiveLoop();

    // Callback when a raw message is received from the server
    std::function<void(const std::string&)> onMessage;

private:
    SOCKET clientSocket = INVALID_SOCKET;
    bool running = false;
};
