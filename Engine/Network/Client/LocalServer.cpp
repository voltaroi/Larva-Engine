#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <cstdlib>
#include <algorithm>
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

// Chemin complet de l'exécutable serveur : à côté du client, sinon dans ..\Server\ ("" si introuvable)
static std::string findExe(const std::string &exeName)
{
    std::string dir = exeDirectory();
    std::string candidates[] = {dir + "\\" + exeName, dir + "\\..\\Server\\" + exeName};
    for (const auto &c : candidates)
        if (fileExists(c))
        {
            char full[MAX_PATH] = {0};
            return GetFullPathNameA(c.c_str(), MAX_PATH, full, nullptr) ? std::string(full) : c;
        }
    return "";
}

bool LocalServer::launch(const std::string &exeName, const std::vector<std::string> &args, std::string &error)
{
    if (isRunning())
        return true;

    std::string exe = findExe(exeName);
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

std::vector<LocalServer::Address> LocalServer::localAddresses()
{
    std::vector<Address> result;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return result;
    char host[256] = {0};
    addrinfo hints{}, *list = nullptr;
    hints.ai_family = AF_INET;
    if (gethostname(host, sizeof(host)) == 0 && getaddrinfo(host, nullptr, &hints, &list) == 0)
    {
        for (addrinfo *a = list; a; a = a->ai_next)
        {
            const sockaddr_in *sin = (const sockaddr_in *)a->ai_addr;
            unsigned char *b = (unsigned char *)&sin->sin_addr;
            char buf[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
            if (b[0] == 127 || (b[0] == 169 && b[1] == 254))
                continue; // boucle locale, adresse automatique sans réseau
            Address addr{buf, "reseau local", true};
            if (b[0] == 25)
                addr.network = "Hamachi";
            else if (b[0] == 26)
                addr.network = "Radmin VPN";
            else if (b[0] == 100 && b[1] >= 64 && b[1] <= 127)
                addr.network = "Tailscale";
            else if (b[0] == 10 && b[1] == 147)
                addr.network = "ZeroTier";
            else
                addr.vpn = false;
            bool duplicate = false;
            for (const auto &r : result)
                duplicate = duplicate || r.ip == addr.ip;
            if (!duplicate)
                result.push_back(addr);
        }
        freeaddrinfo(list);
    }
    WSACleanup();
    // Les adresses VPN d'abord : ce sont celles que les amis à distance doivent utiliser
    std::stable_sort(result.begin(), result.end(), [](const Address &x, const Address &y) { return x.vpn && !y.vpn; });
    return result;
}

std::string LocalServer::shareText()
{
    std::string text;
    for (const auto &a : localAddresses())
        text += (text.empty() ? "" : "  |  ") + a.network + " " + a.ip;
    return text.empty() ? localIp() : text;
}

// Lance une commande sans fenêtre et attend son code de retour (-1 si impossible)
static int runHidden(const std::string &file, const std::string &params, bool elevated)
{
    SHELLEXECUTEINFOA info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = elevated ? "runas" : "open";
    info.lpFile = file.c_str();
    info.lpParameters = params.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExA(&info) || !info.hProcess)
        return -1;
    WaitForSingleObject(info.hProcess, 60000);
    DWORD code = (DWORD)-1;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    return (int)code;
}

bool LocalServer::ensureFirewallRule(const std::string &ruleName, const std::string &exeName, int port, std::string &error)
{
    std::string exe = findExe(exeName);
    if (exe.empty())
    {
        error = exeName + " introuvable";
        return false;
    }
    // Le nom contient le port : changer de port crée la règle correspondante
    std::string name = "name=\"" + ruleName + " (port " + std::to_string(port) + ")\"";
    // netsh renvoie 0 si une règle de ce nom existe déjà
    if (runHidden("netsh", "advfirewall firewall show rule " + name, false) == 0)
        return true;
    // Supprime d'abord les règles existantes du serveur (dont les blocages créés quand on refuse la
    // fenêtre du pare-feu), puis autorise les connexions entrantes sur tous les profils réseau
    std::string program = "program=\"" + exe + "\"";
    std::string cmd = "/c netsh advfirewall firewall delete rule name=all dir=in " + program +
                      " & netsh advfirewall firewall add rule " + name + " dir=in action=allow " + program +
                      " enable=yes profile=any & netsh advfirewall firewall add rule " + name +
                      " dir=in action=allow protocol=TCP localport=" + std::to_string(port) + " enable=yes profile=any";
    runHidden("cmd.exe", cmd, true);
    if (runHidden("netsh", "advfirewall firewall show rule " + name, false) == 0)
        return true;
    error = "Pare-feu non autorise : les autres joueurs risquent de ne pas pouvoir se connecter";
    return false;
}
