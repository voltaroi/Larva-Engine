#include "Client.h"
#include <ws2tcpip.h>
#include <chrono>

#undef min
#undef max

Client::Client() : running(false), clientSocket(INVALID_SOCKET) {}

Client::~Client() {
    disconnect();
}

bool Client::connectToServer(const std::string &host, int port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    // Adresse sans espaces autour (copiée-collée d'un chat) ; un nom d'hôte est aussi accepté
    size_t first = host.find_first_not_of(" \t\r\n"), last = host.find_last_not_of(" \t\r\n");
    std::string address = first == std::string::npos ? "" : host.substr(first, last - first + 1);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(address.c_str());
    if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
        addrinfo hints{}, *found = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (address.empty() || getaddrinfo(address.c_str(), nullptr, &hints, &found) != 0 || !found) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
            WSACleanup();
            return false;
        }
        serverAddr.sin_addr = ((sockaddr_in *)found->ai_addr)->sin_addr;
        freeaddrinfo(found);
    }

    // Non-blocking connect with a timeout, so a wrong address does not freeze the caller for ~20s
    u_long nonBlocking = 1;
    ioctlsocket(clientSocket, FIONBIO, &nonBlocking);
    connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    fd_set writeSet, errorSet;
    FD_ZERO(&writeSet);
    FD_ZERO(&errorSet);
    FD_SET(clientSocket, &writeSet);
    FD_SET(clientSocket, &errorSet);
    timeval timeout{6, 0}; // les VPN (Hamachi en relais) peuvent être lents à établir la connexion
    int ready = select(0, nullptr, &writeSet, &errorSet, &timeout);

    int socketError = 0;
    int errorLen = sizeof(socketError);
    getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, (char *)&socketError, &errorLen);
    if (ready <= 0 || FD_ISSET(clientSocket, &errorSet) || socketError != 0) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    nonBlocking = 0;
    ioctlsocket(clientSocket, FIONBIO, &nonBlocking);

    int noDelay = 1;
    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay, sizeof(noDelay));

    std::cout << "Connected to server " << host << ":" << port << " (socket=" << clientSocket << ")" << std::endl;

    running = true;
        receiveThread = std::thread(&Client::receiveLoop, this);
    return true;
}

void Client::sendMessage(const std::string &msg) {
    send(clientSocket, msg.c_str(), msg.size(), 0);
}

void Client::sendEvent(const std::string &eventName, const JsonValue &data) {
    std::string json = data.toString();
    std::string line = "EVENT " + eventName + " " + json + "\n";
    sendMessage(line);
}

void Client::disconnect() {
    running = false;
    SOCKET socketToClose = clientSocket;
    if (socketToClose != INVALID_SOCKET) {
        closesocket(socketToClose);
    }
    if (receiveThread.joinable() && receiveThread.get_id() != std::this_thread::get_id()) {
        receiveThread.join();
    }
    clientSocket = INVALID_SOCKET;
    WSACleanup();
}

std::vector<std::string> Client::pollMessages() {
    std::vector<std::string> lines;
    std::lock_guard<std::mutex> lock(queueMutex);
    lines.swap(queue);
    return lines;
}

void Client::clearMessages() {
    std::lock_guard<std::mutex> lock(queueMutex);
    queue.clear();
}

void Client::receiveLoop() {
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

            // Parse EVENT messages: EVENT <name> <json>
            if (line.rfind("EVENT ", 0) == 0) {
                size_t space = line.find(' ', 6);
                if (space != std::string::npos) {
                    std::string eventName = line.substr(6, space - 6);
                    std::string json = line.substr(space + 1);
                    JsonValue data = JsonValue::parse(json);
                    handleEvent(eventName, data);
                }
                continue;
            }

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
            else if (queueEnabled) {
                std::lock_guard<std::mutex> lock(queueMutex);
                queue.push_back(line);
            }
        }
    }

    running = false;
    if (!pending.empty()) {
        std::cout << "[Server leftover] " << pending << std::endl;
    }
    std::cout << "Disconnected from server." << std::endl;
}
