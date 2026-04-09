#pragma once
#include <atomic>
#include <string>
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <functional>
#include "../EventEmitter.h"

class Client : public EventEmitter {
public:
    Client();
    ~Client();

    bool connectToServer(const std::string& host, int port);
    void sendMessage(const std::string& msg);
    void sendEvent(const std::string &eventName, const JsonValue &data) override;
    void disconnect();

    void receiveLoop();

    // Callback when a raw message is received from the server
    std::function<void(const std::string&)> onMessage;

private:
    SOCKET clientSocket = INVALID_SOCKET;
    std::atomic<bool> running{false};
    std::thread receiveThread;
};
