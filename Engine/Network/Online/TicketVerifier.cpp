#include "TicketVerifier.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef int SockLen;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int SOCKET;
typedef socklen_t SockLen;
static const SOCKET INVALID_SOCKET = -1;
static int closesocket(SOCKET s) { return close(s); }
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif
#include <algorithm>
#include <chrono>
#include <fstream>
#include "OnlineProtocol.h"
#include "SecureChannel.h"

using namespace OnlineProto;

namespace
{
    double seconds()
    {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    bool sendAll(SOCKET s, const std::string &data)
    {
        size_t done = 0;
        while (done < data.size())
        {
#ifdef _WIN32
            int n = ::send(s, data.data() + done, (int)(data.size() - done), 0);
#else
            int n = (int)::send(s, data.data() + done, data.size() - done, MSG_NOSIGNAL);
#endif
            if (n <= 0)
                return false;
            done += (size_t)n;
        }
        return true;
    }

    // Attend des données au plus ms millisecondes : 1 prêtes, 0 rien, -1 erreur
    int waitReadable(SOCKET s, int ms)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(s, &set);
        timeval tv{ms / 1000, (ms % 1000) * 1000};
        return select((int)s + 1, &set, nullptr, nullptr, &tv);
    }
}

void TicketVerifier::start(const Config &config)
{
    stop();
    cfg = config;
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    stopping = false;
    worker = std::thread(&TicketVerifier::run, this);
}

void TicketVerifier::stop()
{
    if (!worker.joinable())
        return;
    stopping = true;
    worker.join();
    isConnected = false;
}

void TicketVerifier::verify(uint64_t tag, const std::string &ticket, const std::string &audience)
{
    std::lock_guard<std::mutex> lock(mutex);
    bool valid = !ticket.empty() && ticket.size() <= 64 && !audience.empty() && audience.size() <= 96;
    for (char c : ticket + audience)
        if (c == '\t' || c == '\n' || c == '\r')
            valid = false;
    if (!valid)
    {
        Result r;
        r.tag = tag;
        r.error = "Ticket invalide";
        results.push_back(r);
        return;
    }
    requests.push_back({tag, ticket, audience, seconds() + cfg.timeout});
}

std::vector<TicketVerifier::Result> TicketVerifier::poll()
{
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<Result> out;
    out.swap(results);
    return out;
}

std::string TicketVerifier::lastProblem()
{
    std::lock_guard<std::mutex> lock(mutex);
    return problem;
}

void TicketVerifier::fail(const std::string &ticket, const std::string &error)
{
    for (auto it = requests.begin(); it != requests.end(); ++it)
        if (it->ticket == ticket)
        {
            Result r;
            r.tag = it->tag;
            r.error = error;
            results.push_back(r);
            requests.erase(it);
            return;
        }
}

void TicketVerifier::run()
{
    while (!stopping)
    {
        // Connexion et poignée de main chiffrée
        SOCKET s = INVALID_SOCKET;
        addrinfo hints{}, *found = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        if (getaddrinfo(cfg.server.c_str(), std::to_string(cfg.port).c_str(), &hints, &found) == 0)
        {
            for (addrinfo *a = found; a && s == INVALID_SOCKET; a = a->ai_next)
            {
                s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
                if (s != INVALID_SOCKET && connect(s, a->ai_addr, (SockLen)a->ai_addrlen) != 0)
                {
                    closesocket(s);
                    s = INVALID_SOCKET;
                }
            }
            freeaddrinfo(found);
        }
        std::string why = "Larva Online injoignable (" + cfg.server + ":" + std::to_string(cfg.port) + ")";
        SecureChannel ch;
        if (s != INVALID_SOCKET)
        {
            uint8_t reply[SecureChannel::REPLY_SIZE];
            size_t got = 0;
            bool ok = sendAll(s, ch.clientHello());
            double deadline = seconds() + 10.0;
            while (ok && got < sizeof(reply))
            {
                int r = waitReadable(s, 200);
                if (r < 0 || stopping || seconds() > deadline)
                    ok = false;
                else if (r > 0)
                {
                    int n = (int)recv(s, (char *)reply + got, (int)(sizeof(reply) - got), 0);
                    if (n <= 0)
                        ok = false;
                    else
                        got += (size_t)n;
                }
            }
            uint8_t key[32];
            if (ok && ch.clientFinish(reply, key))
            {
                std::string hex = larva::crypto::toHex(key, 32);
                if (!cfg.serverKey.empty())
                {
                    if (lower(cfg.serverKey) != hex)
                        ok = false, why = "Cle du serveur Larva Online incorrecte (serveur usurpe ?)";
                }
                else if (!cfg.knownKeysFile.empty())
                {
                    // Première connexion : la clé est retenue, puis exigée
                    std::ifstream f(cfg.knownKeysFile);
                    std::string server, known;
                    int port = 0;
                    bool seen = false;
                    while (f >> server >> port >> known)
                        if (server == cfg.server && port == cfg.port)
                        {
                            seen = true;
                            if (known != hex)
                                ok = false, why = "La cle du serveur Larva Online a change : refus (supprime " + cfg.knownKeysFile + " si c'est normal)";
                        }
                    f.close();
                    if (!seen)
                        std::ofstream(cfg.knownKeysFile, std::ios::app) << cfg.server << " " << cfg.port << " " << hex << "\n";
                }
            }
            else
                ok = false;
            if (ok)
                ok = sendAll(s, ch.seal(commandBody({"HELLO", std::to_string(VERSION), cfg.game})));
            if (!ok)
            {
                closesocket(s);
                s = INVALID_SOCKET;
            }
        }

        if (s != INVALID_SOCKET)
        {
            int one = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
            {
                std::lock_guard<std::mutex> lock(mutex);
                problem.clear();
                for (Request &r : requests)
                    r.sent = false;
            }
            isConnected = true;
            std::string buffer, cipher, frame;
            double lastPing = seconds();
            char chunk[16384];
            while (!stopping)
            {
                // Vérifications à envoyer
                std::string out;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    for (Request &r : requests)
                        if (!r.sent)
                        {
                            r.sent = true;
                            out += ch.seal(commandBody({"VERIFY_TICKET", r.ticket, r.audience}));
                        }
                    double t = seconds();
                    while (!requests.empty() && requests.front().deadline < t)
                        fail(requests.front().ticket, "Larva Online ne repond pas");
                }
                if (seconds() - lastPing > 15.0)
                {
                    lastPing = seconds();
                    out += ch.seal(commandBody({"PING"}));
                }
                if (!out.empty() && !sendAll(s, out))
                    break;
                int r = waitReadable(s, 20);
                if (r < 0)
                    break;
                if (r == 0)
                    continue;
                int n = (int)recv(s, chunk, sizeof(chunk), 0);
                if (n <= 0)
                    break;
                buffer.append(chunk, (size_t)n);
                size_t offset = 0;
                int status;
                bool bad = false;
                while ((status = nextFrame(buffer, offset, cipher)) == 1)
                {
                    if (!ch.open(cipher, frame) || frame.empty())
                    {
                        bad = true;
                        break;
                    }
                    if (frame[0] != 'C')
                        continue;
                    std::vector<std::string> f = split(frame.substr(1));
                    std::lock_guard<std::mutex> lock(mutex);
                    if (f[0] == "TICKET_OK" && f.size() >= 5)
                    {
                        for (auto it = requests.begin(); it != requests.end(); ++it)
                            if (it->ticket == f[1])
                            {
                                Result res;
                                res.tag = it->tag;
                                res.ok = true;
                                res.user = std::strtoull(f[2].c_str(), nullptr, 10);
                                res.name = f[3];
                                res.game = f[4];
                                results.push_back(res);
                                requests.erase(it);
                                break;
                            }
                    }
                    else if (f[0] == "TICKET_BAD" && f.size() >= 3)
                        fail(f[1], f[2]);
                    else if (f[0] == "ERR" && f.size() >= 3)
                        problem = f[2];
                }
                buffer.erase(0, offset);
                if (status < 0 || bad)
                    break;
            }
            isConnected = false;
            closesocket(s);
            why = "Connexion a Larva Online perdue";
        }

        // Hors ligne : les vérifications en cours échouent, nouvel essai dans 3 s
        {
            std::lock_guard<std::mutex> lock(mutex);
            problem = why;
            while (!requests.empty())
                fail(requests.front().ticket, why);
        }
        for (int i = 0; i < 30 && !stopping; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
