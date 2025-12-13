#pragma once
#include "EventEmitter.h"
#include <iostream>

class Server;  // Forward declaration

class ServerChat {
public:
    ServerChat();
    
    // Initialize the server chat listener
    void init(Server *server);
    
    // Enable/disable chat
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    
    // Toggle chat
    void toggle() { enabled_ = !enabled_; }
    
private:
    bool enabled_ = false;
    Server *serverPtr = nullptr;
};
