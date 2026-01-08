#include "Server.h"
#include "../ServerChat.h"
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

    // Initialize server chat
    if (!serverChat) {
        serverChat = new ServerChat();
        serverChat->init(this);
        serverChat->setEnabled(true);
    }

    std::thread([this]() {
        while (running) {
            SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket != INVALID_SOCKET) {
                std::thread(&Server::clientHandler, this, clientSocket).detach();
            } else {
                std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            }
        }
    }).detach();

    return true;
}

void Server::broadcast(const std::string &msg) {
    std::vector<ClientInfo> targets;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        targets = clients;
    }

    for (const auto &c : targets) {
        if (c.sock == INVALID_SOCKET) continue;
        int res = send(c.sock, msg.c_str(), static_cast<int>(msg.size()), 0);
        if (res == SOCKET_ERROR) {
            std::cerr << "Failed to send to socket " << c.sock << " (id=" << c.id << "): " << WSAGetLastError() << std::endl;
        } else {
            if (msg.rfind("POS ", 0) != 0 && msg.rfind("STATE ", 0) != 0) {
                // std::cout << "Sent to socket " << c.sock << " (id=" << c.id << "): bytes=" << res << " msg='" << msg << "'" << std::endl;
            }
        }
    }
}

bool Server::sendTo(int clientId, const std::string &msg) {
    SOCKET targetSock = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const auto &c : clients) {
            if (c.id == clientId) {
                targetSock = c.sock;
                break;
            }
        }
    }

    if (targetSock == INVALID_SOCKET) {
        std::cerr << "sendTo: no client with id=" << clientId << std::endl;
        return false;
    }

    int res = send(targetSock, msg.c_str(), static_cast<int>(msg.size()), 0);
    if (res == SOCKET_ERROR) {
        std::cerr << "Failed to send to client id=" << clientId << " socket=" << targetSock << ": " << WSAGetLastError() << std::endl;
        return false;
    }
    if (msg.rfind("POS ", 0) != 0 && msg.rfind("STATE ", 0) != 0) {
        // std::cout << "Sent to client id=" << clientId << " socket=" << targetSock << ": bytes=" << res << " msg='" << msg << "'" << std::endl;
    }
    return true;
}

void Server::broadcastEvent(const std::string &eventName, const JsonValue &data) {
    std::string json = data.toString();
    std::string line = "EVENT " + eventName + " " + json + "\n";
    broadcast(line);
}

void Server::sendEvent(const std::string &eventName, const JsonValue &data) {
    broadcastEvent(eventName, data);
}

void Server::clientHandler(SOCKET client) {
    // std::cout << "clientHandler started for socket " << client << std::endl;
    // std::cout << "clientHandler waiting for clientsMutex for socket " << client << std::endl;
    int myId = 0;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        // std::cout << "clientHandler acquired clientsMutex for socket " << client << std::endl;
        myId = ++nextId;
        clients.push_back({client, myId});
        std::cout << "Added client id=" << myId << " socket=" << client << std::endl;
    }

    if (onConnect) {
        onConnect(myId);
    }

    char buffer[512];
    while (true) {
        int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        std::string s(buffer);
        // std::cout << "Received from client: " << s << std::endl;

        // Parse EVENT messages: EVENT <name> <json>
        if (s.rfind("EVENT ", 0) == 0) {
            size_t space = s.find(' ', 6);
            if (space != std::string::npos) {
                std::string eventName = s.substr(6, space - 6);
                std::string json = s.substr(space + 1);
                // Trim trailing whitespace/newline
                while (!json.empty() && (json.back() == '\n' || json.back() == '\r' || std::isspace(json.back()))) {
                    json.pop_back();
                }
                JsonValue data = JsonValue::parse(json);
                handleEvent(eventName, data);
            }
            continue;
        }

        if (s.rfind("INP ", 0) == 0) {
            int seq = 0;
            float dx = 0.0f, dz = 0.0f;
            std::string rest = s.substr(4);
            std::sscanf(rest.c_str(), "%d %f %f", &seq, &dx, &dz);

            if (onInput) {
                onInput(myId, seq, dx, dz);
            }
        }
    }

    int leavingId = -1;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(), [&](const ClientInfo &c) {
            if (c.sock == client) { leavingId = c.id; return true; }
            return false;
        }), clients.end());
    }

    if (leavingId == -1) {
        std::cout << "Disconnect cleanup: socket not found " << client << std::endl;
    } else {
        // std::cout << "Disconnect cleanup: removed client id=" << leavingId << " socket=" << client << std::endl;
    }

    if (leavingId != -1 && onDisconnect) {
        onDisconnect(leavingId);
    }

    closesocket(client);
    std::cout << "Client disconnected id=" << leavingId << std::endl;
}

void Server::stop() {
    running = false;
    closesocket(listenSocket);
    WSACleanup();
}
