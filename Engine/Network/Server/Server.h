#pragma once
#include "../SocketCompat.h"
#include <vector>
#include <string>
#include <mutex>
#include <functional>
#include "../EventEmitter.h"

class ServerChat;  // Forward declaration

class Server : public EventEmitter {
public:
    bool start(int port);
    void broadcast(const std::string &msg);
    bool sendTo(int clientId, const std::string &msg);
    void broadcastEvent(const std::string &eventName, const JsonValue &data);
    void sendEvent(const std::string &eventName, const JsonValue &data) override;
    void stop();
    bool isRunning() const { return running; }

    using ConnectCallback = std::function<void(int clientId)>;
    using InputCallback = std::function<void(int clientId, int seq, float dx, float dz)>;
    using DisconnectCallback = std::function<void(int clientId)>;

    void setOnConnect(ConnectCallback cb) { onConnect = std::move(cb); }
    void setOnInput(InputCallback cb) { onInput = std::move(cb); }
    void setOnDisconnect(DisconnectCallback cb) { onDisconnect = std::move(cb); }
    
    // Chat management
    ServerChat* getChat() { return serverChat; }

private:
    ServerChat *serverChat = nullptr;

private:
    SOCKET listenSocket = INVALID_SOCKET;

    struct ClientInfo {
        SOCKET sock;
        int id;
    };

    std::vector<ClientInfo> clients;
    std::mutex clientsMutex;
    bool running = false;
    int nextId = 0;

    ConnectCallback onConnect;
    InputCallback onInput;
    DisconnectCallback onDisconnect;

    void clientHandler(SOCKET client);
};
