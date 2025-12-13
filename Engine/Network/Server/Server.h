#pragma once
#include <winsock2.h>
#include <vector>
#include <string>
#include <mutex>
#include "../EventEmitter.h"

class ServerChat;  // Forward declaration

class Server : public EventEmitter {
public:
    bool start(int port);
    void broadcast(const std::string &msg);
    void broadcastEvent(const std::string &eventName, const JsonValue &data);
    void sendEvent(const std::string &eventName, const JsonValue &data) override;
    void stop();
    
    // Chat management
    ServerChat* getChat() { return serverChat; }

private:
    ServerChat *serverChat = nullptr;

private:
    SOCKET listenSocket = INVALID_SOCKET;

    struct PlayerInfo {
        SOCKET sock;
        int id;
        int r, g, b;
        float x, y;
        float vx, vy;
        int inputMask;
        int lastProcessedSeq;
    };

    std::vector<PlayerInfo> players;
    std::mutex clientsMutex;
    bool running = false;
    int nextId = 0;

    void clientHandler(SOCKET client);
};
