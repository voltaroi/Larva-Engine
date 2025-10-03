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
    std::vector<SOCKET> clients;
    std::mutex clientsMutex;
    bool running = false;

    void clientHandler(SOCKET client);
};
