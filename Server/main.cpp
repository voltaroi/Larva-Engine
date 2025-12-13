#include <iostream>
#include <thread>
#include <chrono>
#include "Engine/Network/Server/Server.h"
#include "Engine/Network/ServerChat.h"

int main(int argc, char **argv)
{
    Server server;
    if (server.start(3000)) {
        // Enable chat (uncomment to disable)
        if (server.getChat()) {
            server.getChat()->setEnabled(true);
            std::cout << "Chat enabled on server." << std::endl;
        }
        
        std::cout << "Press Ctrl+C to stop server." << std::endl;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    return 0;
}