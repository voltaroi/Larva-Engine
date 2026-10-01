#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdlib>
#include "LocalServer.h"

static PROCESS_INFORMATION serverProcess{};
static bool started = false;

static std::string exeDirectory()
{
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string dir(path);
    size_t pos = dir.find_last_of("\\/");
    return pos == std::string::npos ? "." : dir.substr(0, pos);
}

static bool fileExists(const std::string &path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool LocalServer::isRunning()
{
    if (!started)
        return false;
    DWORD code = 0;
    return GetExitCodeProcess(serverProcess.hProcess, &code) && code == STILL_ACTIVE;
}

bool LocalServer::launch(const std::string &exeName, const std::vector<std::string> &args, std::string &error)
{
    if (isRunning())
        return true;

    std::string dir = exeDirectory();
    std::string candidates[] = {dir + "\\" + exeName, dir + "\\..\\Server\\" + exeName};
    std::string exe;
    for (const auto &c : candidates)
        if (fileExists(c))
        {
            exe = c;
            break;
        }
    if (exe.empty())
    {
        error = exeName + " introuvable";
        return false;
    }

    std::string cmd = "\"" + exe + "\"";
    for (const auto &a : args)
        cmd += " " + a;
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &serverProcess))
    {
        error = "Impossible de lancer " + exeName;
        return false;
    }
    started = true;

    static bool registered = false;
    if (!registered)
    {
        std::atexit(LocalServer::stop);
        registered = true;
    }
    return true;
}

void LocalServer::stop()
{
    if (!started)
        return;
    TerminateProcess(serverProcess.hProcess, 0);
    CloseHandle(serverProcess.hProcess);
    CloseHandle(serverProcess.hThread);
    started = false;
}

std::string LocalServer::localIp()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return "?";
    std::string result = "127.0.0.1";

    // Une socket UDP "connectée" ne transmet rien, mais le système choisit l'interface
    // réseau qu'il utiliserait pour sortir : c'est l'adresse à donner aux autres joueurs.
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s != INVALID_SOCKET)
    {
        sockaddr_in remote{};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(53);
        inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
        sockaddr_in local{};
        int len = sizeof(local);
        if (connect(s, (sockaddr *)&remote, sizeof(remote)) == 0 &&
            getsockname(s, (sockaddr *)&local, &len) == 0)
        {
            char buf[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
            if (std::string(buf) != "0.0.0.0")
                result = buf;
        }
        closesocket(s);
    }

    WSACleanup();
    return result;
}
