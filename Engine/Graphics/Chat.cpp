#include "Chat.h"
#include "../Network/Client/Client.h"
#include "UI.h"
#include <iostream>

Chat::Chat() : visible(false), inputActive(false), myPlayerId(-1) {}

void Chat::init(Client &client) {
    clientPtr = &client;
    
    // Listen for incoming chat messages from server
    client.on("receiveChat", [this](const JsonValue &data) {
        std::string sender = data["sender"].strVal;
        std::string text = data["text"].strVal;
        receiveMessage(sender, text);
    });
}

void Chat::handleKey(unsigned char key) {
    if (!visible) return;
    
    if (key == 13) {  // Enter
        if (!inputBuffer.empty()) {
            sendMessage(inputBuffer, myPlayerId);
            inputBuffer.clear();
        }
        inputActive = false;
    } else if (key == 8) {  // Backspace
        if (!inputBuffer.empty()) {
            inputBuffer.pop_back();
        }
    } else if (key == 27) {  // Escape
        visible = false;
        inputActive = false;
        inputBuffer.clear();
    } else if (key >= 32 && key < 127) {  // Printable ASCII
        if (inputBuffer.length() < 100) {  // Limit message length
            inputBuffer += key;
        }
    }
}

void Chat::sendMessage(const std::string &msg, int playerId) {
    if (!clientPtr) return;
    
    JsonValue data = JsonValue::object();
    data["sender"] = "Player_" + std::to_string(playerId);
    data["text"] = msg;
    data["playerId"] = playerId;
    
    clientPtr->sendEvent("sendChat", data);
    
}

void Chat::receiveMessage(const std::string &sender, const std::string &msg) {
    ChatMessage cm;
    cm.sender = sender;
    cm.text = msg;
    messages.push_back(cm);
    
    // Keep only last MAX_MESSAGES
    if (messages.size() > MAX_MESSAGES) {
        messages.pop_front();
    }
    
    std::cout << "[CHAT] " << sender << ": " << msg << std::endl;
}

void Chat::draw(int screenWidth, int screenHeight) {
    if (!visible) return;
    
    // Draw semi-transparent background
    UI::drawBox(10, screenHeight - 250, screenWidth - 20, 240, 0.1f, 0.1f, 0.1f, 0.8f, false, 0);
    
    // Draw messages
    int y = screenHeight - 60;
    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        std::string line = it->sender + ": " + it->text;
        UI::drawText(20, y, line.c_str());
        y -= 20;
        if (y < screenHeight - 240) break;
    }
    
    // Draw input box
    UI::drawBox(10, screenHeight - 45, screenWidth - 20, 35, 0.2f, 0.2f, 0.2f, 0.9f, false, 0);
    UI::drawText(20, screenHeight - 35, ("Type: " + inputBuffer + (inputActive ? "|" : "")).c_str());
}

void Chat::toggleVisible() {
    visible = !visible;
    if (visible) {
        inputActive = true;
        inputBuffer.clear();
    } else {
        inputActive = false;
        inputBuffer.clear();
    }
}
