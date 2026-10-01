#pragma once
#include <atomic>
#include <string>
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <functional>
#include <mutex>
#include <vector>
#include "../EventEmitter.h"

class Client : public EventEmitter {
public:
    Client();
    ~Client();

    bool connectToServer(const std::string& host, int port);
    void sendMessage(const std::string& msg);
    void sendEvent(const std::string &eventName, const JsonValue &data) override;
    void disconnect();
    // False once the connection has been closed (by us or by the server)
    bool isConnected() const { return running; }

    void receiveLoop();

    // Callback when a raw message is received from the server (called on the receive thread)
    std::function<void(const std::string&)> onMessage;

    // Message queue: when enabled, raw lines not consumed by onMessage are stored and can be
    // read from the game loop (main thread) with pollMessages()
    void enableMessageQueue(bool enabled = true) { queueEnabled = enabled; }
    std::vector<std::string> pollMessages();
    void clearMessages();

private:
    std::atomic<bool> queueEnabled{false};
    std::mutex queueMutex;
    std::vector<std::string> queue;

    SOCKET clientSocket = INVALID_SOCKET;
    std::atomic<bool> running{false};
    std::thread receiveThread;
};
