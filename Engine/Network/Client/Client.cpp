#include "Client.h"

Client::Client() : running(false), clientSocket(INVALID_SOCKET) {}

Client::~Client() {
    disconnect();
}

bool Client::connectToServer(const std::string &host, int port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) return false;

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        closesocket(clientSocket);
        return false;
    }

    running = true;
    std::thread(&Client::receiveLoop, this).detach();
    return true;
}

void Client::sendMessage(const std::string &msg) {
    send(clientSocket, msg.c_str(), msg.size(), 0);
}

void Client::disconnect() {
    running = false;
    closesocket(clientSocket);
    WSACleanup();
}

void Client::receiveLoop() {   // UNE SEULE définition
    char buffer[512];
    while (running) {
        int bytes = recv(clientSocket, buffer, sizeof(buffer)-1, 0);
        if (bytes <= 0) break;

        buffer[bytes] = '\0';
        std::string msg(buffer);
        std::cout << "[Server] " << msg << std::endl;

        // Ici tu peux parser le message pour créer des cubes côté client
    }
    std::cout << "Disconnected from server." << std::endl;
}
