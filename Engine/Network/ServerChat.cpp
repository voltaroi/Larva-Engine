#include "ServerChat.h"
#include "Server/Server.h"

ServerChat::ServerChat() : enabled_(false), serverPtr(nullptr) {}

void ServerChat::init(Server *server) {
    serverPtr = server;
    
    if (!serverPtr) return;
    
    // Register listener for sendChat events from clients
    serverPtr->on("sendChat", [this](const JsonValue &data) {
        if (!enabled_) return;  // Ignore chat if disabled
        
        std::string sender = data["sender"].strVal;
        std::string text = data["text"].strVal;
        std::cout << "[SERVER CHAT] " << sender << ": " << text << std::endl;
        
        // Broadcast the message to all clients
        JsonValue response = JsonValue::object();
        response["sender"] = sender;
        response["text"] = text;
        serverPtr->broadcastEvent("receiveChat", response);
    });
}
