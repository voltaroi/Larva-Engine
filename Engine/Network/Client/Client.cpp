#include "Client.h"
#include <chrono>

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

    std::cout << "Connected to server " << host << ":" << port << " (socket=" << clientSocket << ")" << std::endl;

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
    std::string pending;
    using clock = std::chrono::steady_clock;
    auto lastPosLog = clock::time_point::min();
    while (running) {
        int bytes = recv(clientSocket, buffer, sizeof(buffer)-1, 0);
        if (bytes <= 0) {
            std::cerr << "recv returned " << bytes << " (socket=" << clientSocket << ")" << std::endl;
            break;
        }

        buffer[bytes] = '\0';
        pending.append(buffer, bytes);

        // Traiter chaque ligne terminée par '\n'
        size_t pos;
        while ((pos = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, pos);
            pending.erase(0, pos + 1);

            if (line.empty()) continue;

            // If it's a POS message, throttle console printing to at most once every 5 seconds
            if (line.rfind("POS ", 0) == 0) {
                auto now = clock::now();
                if (now - lastPosLog > std::chrono::seconds(5)) {
                    // std::cout << "[Server] " << line << std::endl;
                    lastPosLog = now;
                }
            } else {
                // std::cout << "[Server] " << line << std::endl;
            }

            if (onMessage) onMessage(line);
        }
    }

    if (!pending.empty()) {
        std::cout << "[Server leftover] " << pending << std::endl;
    }
    std::cout << "Disconnected from server." << std::endl;
}
