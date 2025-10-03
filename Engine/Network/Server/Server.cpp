#include "Server.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <cstdlib>
#include <ctime>

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

    srand(static_cast<unsigned int>(time(nullptr))); // pour RGB aléatoire

    // Boucle principale d'acceptation des clients
    std::thread([this]() {
        while (running) {
            SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket != INVALID_SOCKET) {
                std::cout << "New client connected." << std::endl;
                std::thread(&Server::clientHandler, this, clientSocket).detach();
            }
        }
    }).detach();

    return true;
}

void Server::broadcast(const std::string &msg) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (SOCKET client : clients) {
        send(client, msg.c_str(), static_cast<int>(msg.size()), 0);
    }
}

void Server::clientHandler(SOCKET client) {
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.push_back(client);

        int r = rand() % 256;
        int g = rand() % 256;
        int b = rand() % 256;

        std::string msg = "new player:" + std::to_string(r) + "," +
                          std::to_string(g) + "," + std::to_string(b);

        broadcast(msg);
    }

    char buffer[512];
    while (true) {
        int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        std::cout << "Received from client: " << buffer << std::endl;
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
    }

    closesocket(client);
    std::cout << "Client disconnected." << std::endl;
}

void Server::stop() {
    running = false;
    closesocket(listenSocket);
    WSACleanup();
}
