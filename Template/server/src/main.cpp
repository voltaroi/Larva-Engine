#include <iostream>
#include <thread>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include "Engine/Network/Server/Server.h"
#include "Engine/Network/ServerChat.h"

int main(int argc, char **argv)
{
    Server server;
    int port = 3000;
    if (argc >= 2) {
        try {
            int p = std::stoi(argv[1]);
            if (p > 0 && p <= 65535) port = p;
        } catch (...) {
            std::cerr << "Warning: invalid port argument '" << argv[1] << "', using default 3000" << std::endl;
        }
    }
    struct Player {
        int id;
        int r, g, b;
        float x, y;
        float vx, vy;
        int lastProcessedSeq;
    };

    std::vector<Player> players;
    std::mutex playersMutex;

    server.setOnConnect([&](int clientId) {
        Player newPlayer{};
        newPlayer.id = clientId;
        newPlayer.r = (clientId * 97) % 256;
        newPlayer.g = (clientId * 57) % 256;
        newPlayer.b = (clientId * 31) % 256;
        newPlayer.x = 0.0f;
        newPlayer.y = 0.0f;
        newPlayer.vx = 0.0f;
        newPlayer.vy = 0.0f;
        newPlayer.lastProcessedSeq = 0;

        std::vector<Player> existing;
        {
            std::lock_guard<std::mutex> lock(playersMutex);
            existing = players;
            players.push_back(newPlayer);
        }

        for (const auto &ep : existing) {
            char bufExist[256];
            std::snprintf(bufExist, sizeof(bufExist), "EXIST %d %d %d %d %f %f\n", ep.id, ep.r, ep.g, ep.b, ep.x, ep.y);
            server.sendTo(clientId, bufExist);
        }

        char bufAssign[128];
        std::snprintf(bufAssign, sizeof(bufAssign), "ASSIGN %d %d %d %d\n", newPlayer.id, newPlayer.r, newPlayer.g, newPlayer.b);
        server.sendTo(clientId, bufAssign);

        char bufJoin[128];
        std::snprintf(bufJoin, sizeof(bufJoin), "JOIN %d %d %d %d\n", newPlayer.id, newPlayer.r, newPlayer.g, newPlayer.b);
        server.broadcast(bufJoin);
    });

    server.setOnInput([&](int clientId, int seq, float dx, float dz) {
        std::lock_guard<std::mutex> lock(playersMutex);
        for (auto &p : players) {
            if (p.id == clientId) {
                p.vx = dx;
                p.vy = dz;
                p.lastProcessedSeq = seq;
                break;
            }
        }
    });

    server.setOnDisconnect([&](int clientId) {
        {
            std::lock_guard<std::mutex> lock(playersMutex);
            players.erase(std::remove_if(players.begin(), players.end(), [&](const Player &p) { return p.id == clientId; }), players.end());
        }

        char bufLeave[64];
        std::snprintf(bufLeave, sizeof(bufLeave), "LEAVE %d\n", clientId);
        server.broadcast(bufLeave);
    });

    std::cout << "Starting server on port " << port << std::endl;
    if (server.start(port)) {
        // Enable chat (uncomment to disable)
        if (server.getChat()) {
            server.getChat()->setEnabled(true);
            std::cout << "Chat enabled on server." << std::endl;
        }

        std::cout << "Press Ctrl+C to stop server." << std::endl;

        const std::chrono::milliseconds tickDuration(50);
        while (server.isRunning()) {
            auto tickStart = std::chrono::steady_clock::now();

            std::vector<Player> snapshot;
            {
                std::lock_guard<std::mutex> lock(playersMutex);
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
                server.broadcast(stateMsg);
            }

            auto elapsed = std::chrono::steady_clock::now() - tickStart;
            if (elapsed < tickDuration) {
                std::this_thread::sleep_for(tickDuration - elapsed);
            }
        }
    }
    return 0;
}