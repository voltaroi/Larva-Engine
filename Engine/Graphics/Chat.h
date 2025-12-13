#pragma once
#include <string>
#include <vector>
#include <deque>

class Chat {
public:
    Chat();
    
    // Initialize chat (set up listeners on client)
    void init(class Client &client);
    
    // Handle keyboard input for chat
    void handleKey(unsigned char key);
    
    // Set player ID (called when ASSIGN is received)
    void setPlayerId(int id) { myPlayerId = id; }
    
    // Send a message
    void sendMessage(const std::string &msg, int playerId);
    
    // Receive a message (called by event listener)
    void receiveMessage(const std::string &sender, const std::string &msg);
    
    // Draw the chat on screen
    void draw(int screenWidth, int screenHeight);
    
    // Toggle chat visibility
    void toggleVisible();
    bool isVisible() const { return visible; }
    
private:
    struct ChatMessage {
        std::string sender;
        std::string text;
        // color could be added here
    };
    
    bool visible = false;
    bool inputActive = false;
    std::string inputBuffer;
    std::deque<ChatMessage> messages;
    static const size_t MAX_MESSAGES = 10;
    
    Client *clientPtr = nullptr;
    int myPlayerId = -1;
};
