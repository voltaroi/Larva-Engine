#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;

struct WSADATA {};

inline unsigned short MAKEWORD(int low, int high) {
    return static_cast<unsigned short>((low & 0xff) | ((high & 0xff) << 8));
}

inline int WSAStartup(unsigned short, WSADATA*) {
    return 0;
}

inline int WSACleanup() {
    return 0;
}

inline int WSAGetLastError() {
    return errno;
}

inline int closesocket(SOCKET socketHandle) {
    return ::close(socketHandle);
}
#endif