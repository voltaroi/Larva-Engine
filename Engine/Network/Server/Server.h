#pragma once
#include <winsock2.h>
#include <vector>
#include <string>
#include <mutex>

class Server {
public:
    bool start(int port);
    void broadcast(const std::string &msg);
    void stop();

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
