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
    // Transport externe (Steam, relais...) à la place de TCP : sender envoie les données vers le serveur,
    // et receiveData() injecte ce qui en revient (mêmes lignes, même file de messages que par TCP)
    void connectExternal(std::function<void(const std::string&)> sender);
    void receiveData(const std::string &data);
    bool isExternal() const { return externalSend != nullptr; }
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

    void handleLine(const std::string &line);
    std::function<void(const std::string&)> externalSend;
    std::mutex externalMutex;
    std::string externalPending;

    SOCKET clientSocket = INVALID_SOCKET;
    std::atomic<bool> running{false};
    std::thread receiveThread;
};
