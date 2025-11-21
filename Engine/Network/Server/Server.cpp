#include "Server.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <cstdio>

bool Server::start(int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return false;
    }

    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    running = true;
    std::cout << "Server started on port " << port << std::endl;

    std::thread([this]() {
        while (running) {
            SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket != INVALID_SOCKET) {
                std::cout << "New client connected. socket=" << clientSocket << std::endl;
                std::thread(&Server::clientHandler, this, clientSocket).detach();
            }
        }
    }).detach();

    std::thread([this]() {
        using namespace std::chrono;
        const milliseconds tickDuration(50);
        while (running) {
            auto tickStart = steady_clock::now();

            std::vector<PlayerInfo> snapshot;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                        for (auto &p : players) {
                            p.x += p.vx;
                            p.y += p.vy;
                        }

                snapshot = players;
            }

            std::string stateMsg;
            char buf[128];
            for (const auto &p : snapshot) {
                std::snprintf(buf, sizeof(buf), "STATE %d %f %f %d\n", p.id, p.x, p.y, p.lastProcessedSeq);
                stateMsg += buf;
            }

            if (!stateMsg.empty()) {
                broadcast(stateMsg);
            }

            auto elapsed = steady_clock::now() - tickStart;
            if (elapsed < tickDuration) std::this_thread::sleep_for(tickDuration - elapsed);
        }
    }).detach();

    return true;
}

void Server::broadcast(const std::string &msg) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto &p : players) {
        if (p.sock != INVALID_SOCKET) {
            int res = send(p.sock, msg.c_str(), static_cast<int>(msg.size()), 0);
            if (res == SOCKET_ERROR) {
                std::cerr << "Failed to send to socket " << p.sock << " (id=" << p.id << "): " << WSAGetLastError() << std::endl;
            } else {
                if (msg.rfind("POS ", 0) != 0 && msg.rfind("STATE ", 0) != 0) {
                    std::cout << "Sent to socket " << p.sock << " (id=" << p.id << "): bytes=" << res << " msg='" << msg << "'" << std::endl;
                }
            }
        }
    }
}

void Server::clientHandler(SOCKET client) {
    std::cout << "clientHandler started for socket " << client << std::endl;
    int myId = 0;
    std::vector<PlayerInfo> existingPlayersSnapshot;
    std::string joinMsg;
    std::string assignMsg;
    {
        std::cout << "clientHandler acquiring clientsMutex for socket " << client << std::endl;
        std::lock_guard<std::mutex> lock(clientsMutex);
        std::cout << "clientHandler inside clientsMutex for socket " << client << std::endl;

        myId = ++nextId;
        int r = (myId * 97) % 256;
        int g = (myId * 57) % 256;
        int b = (myId * 31) % 256;

        existingPlayersSnapshot = players;

        PlayerInfo pi;
        pi.sock = client;
        pi.id = myId;
        pi.r = r; pi.g = g; pi.b = b;
        pi.x = 0.0f; pi.y = 0.0f;
        pi.vx = 0.0f; pi.vy = 0.0f;
        pi.inputMask = 0;
        pi.lastProcessedSeq = 0;
        players.push_back(pi);
        std::cout << "Added player id=" << myId << " socket=" << client << " (r,g,b)=(" << r << "," << g << "," << b << ")" << std::endl;

        char bufAssign[128];
        std::snprintf(bufAssign, sizeof(bufAssign), "ASSIGN %d %d %d %d\n", myId, r, g, b);
        assignMsg = bufAssign;

        char bufJoin[128];
        std::snprintf(bufJoin, sizeof(bufJoin), "JOIN %d %d %d %d\n", myId, r, g, b);
        joinMsg = bufJoin;
    }

    for (const auto &ep : existingPlayersSnapshot) {
        char bufExist[256];
        std::snprintf(bufExist, sizeof(bufExist), "EXIST %d %d %d %d %f %f\n", ep.id, ep.r, ep.g, ep.b, ep.x, ep.y);
        int res = send(client, bufExist, static_cast<int>(strlen(bufExist)), 0);
        if (res == SOCKET_ERROR) {
            std::cerr << "Failed to send EXIST to new client socket " << client << ": " << WSAGetLastError() << std::endl;
        } else {
            std::cout << "Sent EXIST to new client socket " << client << ": bytes=" << res << " msg='" << bufExist << "'" << std::endl;
        }
    }

    int resAssign = send(client, assignMsg.c_str(), static_cast<int>(assignMsg.size()), 0);
    if (resAssign == SOCKET_ERROR) {
        std::cerr << "Failed to send ASSIGN to client " << client << ": " << WSAGetLastError() << std::endl;
    } else {
        std::cout << "Sent ASSIGN to client " << client << ": bytes=" << resAssign << " msg='" << assignMsg << "'" << std::endl;
    }

    broadcast(joinMsg);

    char buffer[512];
    while (true) {
        int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        std::string s(buffer);
        // std::cout << "Received from client: " << s << std::endl;

        if (s.rfind("INP ", 0) == 0) {
            int seq = 0;
            float dx = 0.0f, dz = 0.0f;
            std::string rest = s.substr(4);
            std::sscanf(rest.c_str(), "%d %f %f", &seq, &dx, &dz);

            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto &p : players) {
                if (p.sock == client) {
                    p.vx = dx;
                    p.vy = dz;
                    p.lastProcessedSeq = seq;
                    break;
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        int leavingId = -1;
        players.erase(std::remove_if(players.begin(), players.end(), [&](const PlayerInfo &p) {
            if (p.sock == client) { leavingId = p.id; return true; }
            return false;
        }), players.end());

        if (leavingId != -1) {
            char bufLeave[64];
            std::snprintf(bufLeave, sizeof(bufLeave), "LEAVE %d", leavingId);
            broadcast(bufLeave);
        }
    }

    closesocket(client);
    std::cout << "Client disconnected." << std::endl;
}

void Server::stop() {
    running = false;
    closesocket(listenSocket);
    WSACleanup();
}
