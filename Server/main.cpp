#include <iostream>
#include <thread>
#include <chrono>
#include "Engine/Network/Server/Server.h"

int main(int argc, char **argv)
{
    Server server;
    if (server.start(3000)) {
        std::cout << "Press Ctrl+C to stop server." << std::endl;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    return 0;
}