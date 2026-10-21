#include "LarvaOnline.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include "Engine/Core/Crypto.h"
#include "OnlineProtocol.h"

using namespace OnlineProto;

namespace
{
    const SOCKET NO_SOCKET = INVALID_SOCKET;

    // Liaisons directes
    const size_t WINDOW = 512;           // morceaux envoyés et pas encore confirmés, au plus
    const size_t BURST = 128;            // nouveaux morceaux envoyés par image, au plus
    const double PUNCH_TIME = 8.0;       // durée d'une tentative de traversée des box
    const double PUNCH_RETRY = 30.0;     // nouvel essai après un échec ou une coupure
    const double PROBE_EVERY = 0.2;
    const double KEEPALIVE = 1.0;        // paquet vide en direct (garde la box ouverte, porte les accusés)
    const double DEAD_SILENCE = 5.0;     // plus rien reçu en direct : retour au relais
    const double UNACKED_LIMIT = 10.0;   // un morceau sans accusé depuis si longtemps (chemin qui ne passe plus) : relais
    const size_t RELAY_BATCH = 256 * 1024;

    double seconds()
    {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    uint64_t toId(const std::string &s) { return std::strtoull(s.c_str(), nullptr, 10); }

    // Comparaison des numéros de morceaux (tolère le passage de 2^32 à 0)
    bool before(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }

    bool makeAddr(const std::string &ip, const std::string &port, uint8_t out[16])
    {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        int p = std::atoi(port.c_str());
        if (p <= 0 || p > 65535 || inet_pton(AF_INET, ip.c_str(), &a.sin_addr) != 1)
            return false;
        a.sin_port = htons((unsigned short)p);
        std::memcpy(out, &a, 16);
        return true;
    }

    bool sameAddr(const uint8_t *a, const uint8_t *b)
    {
        const sockaddr_in *x = (const sockaddr_in *)a, *y = (const sockaddr_in *)b;
        return x->sin_port == y->sin_port && x->sin_addr.s_addr == y->sin_addr.s_addr;
    }
}

LarvaOnline::LarvaOnline(const Online::Config &config) : cfg(config), udpSock((uintptr_t)NO_SOCKET), sock((uintptr_t)NO_SOCKET)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    // Socket UDP des connexions directes : port choisi par le système
    SOCKET u = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in any{};
    any.sin_family = AF_INET;
    any.sin_addr.s_addr = INADDR_ANY;
    if (u != NO_SOCKET && bind(u, (sockaddr *)&any, sizeof(any)) == 0)
    {
        int len = sizeof(any);
        getsockname(u, (sockaddr *)&any, &len);
        udpPort = ntohs(any.sin_port);
        udpSock = (uintptr_t)u;
        // Windows : un "port injoignable" reçu en retour ne doit pas faire échouer les lectures suivantes
        BOOL off = FALSE;
        DWORD bytes = 0;
        WSAIoctl(u, _WSAIOW(IOC_VENDOR, 12), &off, sizeof(off), nullptr, 0, &bytes, nullptr, nullptr);
    }
    else if (u != NO_SOCKET)
        closesocket(u);
    worker = std::thread(&LarvaOnline::run, this);
    if (udpSock != (uintptr_t)NO_SOCKET)
        udpWorker = std::thread(&LarvaOnline::runUdp, this);
}

LarvaOnline::~LarvaOnline()
{
    shutdown();
    WSACleanup();
}

void LarvaOnline::shutdown()
{
    if (!worker.joinable())
        return;
    stopping = true;
    SOCKET s = (SOCKET)sock.load();
    if (s != NO_SOCKET)
        ::shutdown(s, SD_BOTH);
    worker.join();
    if (udpWorker.joinable())
        udpWorker.join();
    if (udpSock != (uintptr_t)NO_SOCKET)
        closesocket((SOCKET)udpSock);
    udpSock = (uintptr_t)NO_SOCKET;
    connected = false;
}

// ---------------------------------------------------------------------------
//  Threads réseau : connexion chiffrée au serveur, réception UDP
// ---------------------------------------------------------------------------

void LarvaOnline::push(Event e)
{
    std::lock_guard<std::mutex> lock(eventsMutex);
    events.push_back(std::move(e));
}

// Poignée de main chiffrée et vérification de la clé du serveur ; fatal : raison d'un refus
bool LarvaOnline::handshake(uintptr_t sockValue, SecureChannel &ch, std::string &fatal)
{
    SOCKET s = (SOCKET)sockValue;
    std::string hello = ch.clientHello();
    if (::send(s, hello.data(), (int)hello.size(), 0) != (int)hello.size())
        return false;
    uint8_t reply[SecureChannel::REPLY_SIZE];
    size_t got = 0;
    double deadline = seconds() + 10.0;
    while (got < sizeof(reply))
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(s, &set);
        timeval tv{0, 200000};
        int r = select(0, &set, nullptr, nullptr, &tv);
        if (r < 0 || stopping || seconds() > deadline)
            return false;
        if (r == 0)
            continue;
        int n = recv(s, (char *)reply + got, (int)(sizeof(reply) - got), 0);
        if (n <= 0)
            return false;
        got += n;
    }
    uint8_t key[32];
    if (!ch.clientFinish(reply, key))
        return false;
    std::string keyHex = larva::crypto::toHex(key, 32);
    if (!cfg.serverKey.empty())
    {
        if (OnlineProto::lower(cfg.serverKey) != keyHex)
        {
            fatal = "Cle du serveur incorrecte : connexion refusee (serveur usurpe ?)";
            return false;
        }
        return true;
    }
    // Pas de clé épinglée : celle de la première connexion est retenue, puis exigée
    std::string path = sessionFolder() + "\\known_servers.txt";
    std::ifstream f(path);
    std::string server, hex;
    int port = 0;
    while (f >> server >> port >> hex)
        if (server == cfg.server && port == cfg.port)
        {
            if (hex == keyHex)
                return true;
            fatal = "La cle du serveur a change : connexion refusee (si c'est normal, supprime known_servers.txt)";
            return false;
        }
    f.close();
    std::ofstream out(path, std::ios::app);
    out << cfg.server << " " << cfg.port << " " << keyHex << "\n";
    return true;
}

void LarvaOnline::run()
{
    while (!stopping)
    {
        SOCKET s = NO_SOCKET;
        sockaddr_in serverAddr{};
        addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        if (getaddrinfo(cfg.server.c_str(), std::to_string(cfg.port).c_str(), &hints, &result) == 0)
        {
            for (addrinfo *a = result; a && s == NO_SOCKET; a = a->ai_next)
            {
                s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
                if (s != NO_SOCKET && connect(s, a->ai_addr, (int)a->ai_addrlen) != 0)
                {
                    closesocket(s);
                    s = NO_SOCKET;
                }
                else if (s != NO_SOCKET)
                    std::memcpy(&serverAddr, a->ai_addr, sizeof(serverAddr));
            }
            freeaddrinfo(result);
        }
        std::string fatal;
        SecureChannel ch;
        if (s != NO_SOCKET && !handshake((uintptr_t)s, ch, fatal))
        {
            closesocket(s);
            s = NO_SOCKET;
            if (!fatal.empty())
            {
                Event e;
                e.kind = Event::Fatal;
                e.data = fatal;
                push(e);
            }
        }
        if (s != NO_SOCKET)
        {
            int one = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
            {
                std::lock_guard<std::mutex> lock(sendMutex);
                channel = ch;
                sock = (uintptr_t)s;
                connected = true;
            }
            // Adresse locale vers le serveur : celle à donner aux joueurs du même réseau
            sockaddr_in local{};
            int len = sizeof(local);
            char ip[64] = "-";
            if (getsockname(s, (sockaddr *)&local, &len) == 0)
                inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
            Event e;
            e.kind = Event::Connected;
            e.fields = {ip};
            std::memcpy(e.addr, &serverAddr, 16);
            push(e);
            sendBody(commandBody({"HELLO", std::to_string(VERSION), cfg.game}));

            std::string buffer, cipher, frame;
            double lastPing = seconds();
            char chunk[65536];
            while (!stopping)
            {
                fd_set set;
                FD_ZERO(&set);
                FD_SET(s, &set);
                timeval tv{1, 0};
                int r = select(0, &set, nullptr, nullptr, &tv);
                if (r < 0)
                    break;
                if (seconds() - lastPing > 15.0)
                {
                    lastPing = seconds();
                    sendBody(commandBody({"PING"}));
                }
                if (r == 0)
                    continue;
                int n = recv(s, chunk, sizeof(chunk), 0);
                if (n <= 0)
                    break;
                buffer.append(chunk, n);
                size_t offset = 0;
                int got;
                bool bad = false;
                while ((got = nextFrame(buffer, offset, cipher)) == 1)
                {
                    if (!channel.open(cipher, frame) || frame.empty())
                    {
                        bad = true;
                        break;
                    }
                    Event ev;
                    if (frame[0] == 'C')
                    {
                        ev.kind = Event::Command;
                        ev.fields = split(frame.substr(1));
                    }
                    else if (frame[0] == 'D' && frame.size() >= 9)
                    {
                        ev.kind = Event::Data;
                        ev.peer = getU64(frame.data() + 1);
                        ev.data = frame.substr(9);
                    }
                    else
                        continue;
                    push(std::move(ev));
                }
                buffer.erase(0, offset);
                if (got < 0 || bad)
                    break;
            }
            {
                std::lock_guard<std::mutex> lock(sendMutex);
                connected = false;
                sock = (uintptr_t)NO_SOCKET;
                closesocket(s);
            }
        }
        Event e;
        e.kind = Event::Disconnected;
        push(e);
        // Nouvel essai dans quelques secondes
        for (int i = 0; i < 50 && !stopping; ++i)
            Sleep(100);
    }
}

void LarvaOnline::runUdp()
{
    SOCKET u = (SOCKET)udpSock;
    char buf[2048];
    while (!stopping)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(u, &set);
        timeval tv{0, 200000};
        if (select(0, &set, nullptr, nullptr, &tv) <= 0)
            continue;
        sockaddr_in from{};
        int len = sizeof(from);
        int n = recvfrom(u, buf, sizeof(buf), 0, (sockaddr *)&from, &len);
        if (n <= 0)
            continue;
        Event e;
        e.kind = Event::Udp;
        e.data.assign(buf, n);
        std::memcpy(e.addr, &from, 16);
        push(std::move(e));
    }
}

bool LarvaOnline::sendBody(const std::string &body)
{
    std::lock_guard<std::mutex> lock(sendMutex);
    SOCKET s = (SOCKET)sock.load();
    if (!connected || s == NO_SOCKET)
        return false;
    std::string bytes = channel.seal(body);
    size_t done = 0;
    while (done < bytes.size())
    {
        int n = ::send(s, bytes.data() + done, (int)(bytes.size() - done), 0);
        if (n <= 0)
            return false;
        done += n;
    }
    return true;
}

void LarvaOnline::command(const std::vector<std::string> &fields)
{
    sendBody(commandBody(fields));
}

// ---------------------------------------------------------------------------
//  Thread principal
// ---------------------------------------------------------------------------

void LarvaOnline::update()
{
    std::deque<Event> batch;
    {
        std::lock_guard<std::mutex> lock(eventsMutex);
        batch.swap(events);
    }
    const double now = seconds();
    for (Event &e : batch)
    {
        switch (e.kind)
        {
        case Event::Connected:
            state_ = State::Connecting;
            localIp = e.fields.empty() ? "-" : e.fields[0];
            std::memcpy(serverUdpAddr, e.addr, 16);
            serverUdpKnown = true;
            break;
        case Event::Disconnected:
            if (state_ != State::Offline)
            {
                resetLobby(true);
                clearLinks();
                friendList.clear();
                me = 0;
                if (joiningLobby && onLobbyEntered)
                    onLobbyEntered(0, false);
                joiningLobby = false;
            }
            udpToken.clear();
            udpSeen = false;
            state_ = State::Offline;
            break;
        case Event::Fatal:
            error = e.data;
            break;
        case Event::Command:
            if (!e.fields.empty())
                handle(e.fields);
            break;
        case Event::Data:
        {
            // Morceaux relayés par le serveur
            auto it = links.find(e.peer);
            if (it != links.end() && members.count(e.peer))
                readChunks(it->second, e.data.data(), e.data.size());
            break;
        }
        case Event::Udp:
            handleUdp(e.addr, e.data, now);
            break;
        }
    }

    // Inscription UDP auprès du serveur : il découvre l'adresse publique (après la box) ; répétée pour garder la box ouverte
    if (connected && !udpToken.empty() && serverUdpKnown && udpSock != (uintptr_t)NO_SOCKET && now >= nextUdpHello)
    {
        std::string hello = std::string(UDP_HELLO) + udpToken;
        sendto((SOCKET)udpSock, hello.data(), (int)hello.size(), 0, (const sockaddr *)serverUdpAddr, 16);
        nextUdpHello = now + (udpSeen ? 15.0 : 1.0);
    }
    tickLinks(now);
}

void LarvaOnline::handle(const std::vector<std::string> &f)
{
    const std::string &op = f[0];
    if (op == "WELCOME")
    {
        std::string name, token;
        loadSession(name, token);
        if (!name.empty() && !token.empty())
        {
            state_ = State::LoggingIn;
            command({"RESUME", name, token});
        }
        else
            state_ = State::NeedLogin;
    }
    else if (op == "UDP_TOKEN" && f.size() >= 2)
    {
        udpToken = f[1];
        udpSeen = false;
        nextUdpHello = 0;
        if (udpPort)
            command({"UDP_LOCAL", localIp, std::to_string(udpPort)});
    }
    else if (op == "UDP_SEEN")
        udpSeen = true;
    else if (op == "PEER_INFO" && f.size() >= 8)
        onPeerInfo(f);
    else if (op == "PEER_ADDR" && f.size() >= 6)
    {
        auto it = links.find(toId(f[1]));
        if (it != links.end())
            setCandidates(it->second, f[2], f[3], f[4], f[5]);
    }
    else if (op == "AUTH" && f.size() >= 4)
    {
        me = toId(f[1]);
        myName_ = f[2];
        names[me] = myName_;
        state_ = State::Ready;
        error.clear();
        if (rememberLogin)
            saveSession(myName_, f[3]);
        if (Online::LobbyId id = deferredJoin)
        {
            deferredJoin = 0;
            joinLobby(id);
        }
    }
    else if (op == "ERR" && f.size() >= 3)
    {
        if (f[1] == "resume")
        {
            saveSession("", "");
            state_ = State::NeedLogin;
        }
        else if (f[1] == "auth" && state_ == State::LoggingIn)
            state_ = State::NeedLogin;
        error = f[2];
    }
    else if ((op == "TICKET" && f.size() >= 3) || (op == "TICKET_FAILED" && f.size() >= 3))
    {
        if (onTicket)
            onTicket(f[1], op == "TICKET" ? f[2] : "", op == "TICKET" ? "" : f[2]);
    }
    else if (op == "FRIEND" && f.size() >= 7)
    {
        Online::Friend fr;
        fr.id = toId(f[1]);
        fr.name = f[2];
        int st = std::atoi(f[3].c_str());
        fr.state = st == ONLINE ? Online::Friend::Online
                 : st == REQUEST_IN ? Online::Friend::RequestIn
                 : st == REQUEST_OUT ? Online::Friend::RequestOut
                                     : Online::Friend::Offline;
        fr.game = f[4] == "-" ? "" : f[4];
        fr.lobby = toId(f[5]);
        fr.joinable = f[6] == "1";
        friendList[fr.id] = fr;
        names[fr.id] = fr.name;
    }
    else if (op == "FRIEND_GONE" && f.size() >= 2)
        friendList.erase(toId(f[1]));
    else if (op == "LOBBY_ENTERED" && f.size() >= 4)
    {
        resetLobby(false);
        lobby = toId(f[1]);
        owner = toId(f[2]);
        for (size_t i = 4; i + 1 < f.size(); i += 2)
        {
            members[toId(f[i])] = f[i + 1];
            names[toId(f[i])] = f[i + 1];
        }
        // Liaisons d'un salon précédent : les PEER_INFO de celui-ci sont arrivés juste avant
        for (auto it = links.begin(); it != links.end();)
        {
            if (members.count(it->first))
                ++it;
            else
            {
                pairs.erase(it->second.pair);
                it = links.erase(it);
            }
        }
        joiningLobby = false;
        if (onLobbyEntered)
            onLobbyEntered(lobby, true);
    }
    else if (op == "LOBBY_FAILED")
    {
        error = f.size() >= 2 ? f[1] : "Impossible de rejoindre la partie";
        joiningLobby = false;
        if (onLobbyEntered)
            onLobbyEntered(0, false);
    }
    else if (op == "MEMBER" && f.size() >= 3 && lobby)
    {
        Online::UserId id = toId(f[1]);
        members[id] = f[2];
        names[id] = f[2];
        if (onMemberJoined)
            onMemberJoined(id);
    }
    else if (op == "LOBBY_OWNER" && f.size() >= 2 && lobby)
    {
        // L'ancien propriétaire est parti (son MEMBER_LEFT suit) : le salon continue avec celui-ci
        owner = toId(f[1]);
        if (onLobbyOwnerChanged)
            onLobbyOwnerChanged(owner);
    }
    else if (op == "MEMBER_LEFT" && f.size() >= 2 && lobby)
    {
        Online::UserId id = toId(f[1]);
        dropLink(id);
        if (members.erase(id) && onMemberLeft)
            onMemberLeft(id);
    }
    else if (op == "LOBBY_CLOSED")
    {
        resetLobby(true);
        clearLinks();
    }
    else if (op == "INVITED" && f.size() >= 5)
    {
        Online::Invite inv;
        inv.from = toId(f[1]);
        inv.fromName = f[2];
        inv.lobby = toId(f[3]);
        inv.game = f[4];
        names[inv.from] = inv.fromName;
        for (auto it = pendingInvites.begin(); it != pendingInvites.end();)
            it = it->from == inv.from ? pendingInvites.erase(it) : it + 1;
        pendingInvites.push_back(inv);
        if (onInviteReceived)
            onInviteReceived(inv);
    }
    else if (op == "PEER_FAILED" && f.size() >= 2)
    {
        if (onSessionFailed)
            onSessionFailed(toId(f[1]));
    }
}

void LarvaOnline::resetLobby(bool notify)
{
    std::vector<Online::UserId> gone;
    for (const auto &m : members)
        if (m.first != me)
            gone.push_back(m.first);
    lobby = 0;
    owner = 0;
    members.clear();
    if (notify && onMemberLeft)
        for (Online::UserId id : gone)
            onMemberLeft(id);
}

// ---------------------------------------------------------------------------
//  Liaisons avec les membres du salon : direct (UDP) ou relais (serveur)
// ---------------------------------------------------------------------------

void LarvaOnline::onPeerInfo(const std::vector<std::string> &f)
{
    Online::UserId peer = toId(f[1]);
    uint8_t key[32];
    if (!larva::crypto::fromHex(f[3], key, 32))
        return;
    dropLink(peer);
    Link &l = links[peer];
    l.peer = peer;
    l.pair = (uint32_t)toId(f[2]);
    pairs[l.pair] = peer;
    // Une clé par sens, dérivée de la clé commune donnée par le serveur
    uint8_t lohi[32], hilo[32];
    larva::crypto::hkdf(nullptr, 0, key, 32, "larva-p2p lo>hi", 15, lohi, 32);
    larva::crypto::hkdf(nullptr, 0, key, 32, "larva-p2p hi>lo", 15, hilo, 32);
    bool low = me < peer;
    std::memcpy(l.sendKey, low ? lohi : hilo, 32);
    std::memcpy(l.recvKey, low ? hilo : lohi, 32);
    setCandidates(l, f[4], f[5], f[6], f[7]);
}

void LarvaOnline::setCandidates(Link &l, const std::string &pubIp, const std::string &pubPort, const std::string &lanIp, const std::string &lanPort)
{
    l.hasPub = makeAddr(pubIp, pubPort, l.pubAddr);
    l.hasLocal = makeAddr(lanIp, lanPort, l.localAddr);
    if (!l.direct)
        l.nextPunch = 0; // nouvelle tentative tout de suite
}

void LarvaOnline::dropLink(Online::UserId peer)
{
    auto it = links.find(peer);
    if (it == links.end())
        return;
    pairs.erase(it->second.pair);
    links.erase(it);
}

void LarvaOnline::clearLinks()
{
    links.clear();
    pairs.clear();
}

void LarvaOnline::debugBlockDirect(bool blocked)
{
    blockDirect = blocked;
    if (!blocked)
        for (auto &e : links)
            e.second.nextPunch = 0;
}

bool LarvaOnline::directConnection(Online::UserId id)
{
    auto it = links.find(id);
    return it != links.end() && it->second.direct;
}

LarvaOnline::LinkStats LarvaOnline::linkStats(Online::UserId peer) const
{
    LinkStats s;
    auto it = links.find(peer);
    if (it == links.end())
        return s;
    s.exists = true;
    s.direct = it->second.direct;
    s.rtt = it->second.srtt;
    s.directBytes = it->second.directBytes;
    s.relayBytes = it->second.relayBytes;
    s.fallbacks = it->second.fallbacks;
    return s;
}

bool LarvaOnline::send(Online::UserId to, const void *data, uint32_t size)
{
    auto it = links.find(to);
    if (!lobby || !members.count(to) || it == links.end() || size > MAX_FRAME - 64)
        return false;
    Link &l = it->second;
    const char *p = (const char *)data;
    if (!l.direct)
    {
        std::deque<Chunk> one(1);
        one[0].seq = l.sendSeq++;
        one[0].flags = CHUNK_LAST;
        one[0].data.assign(p, size);
        relay(l, one);
        return true;
    }
    uint32_t done = 0;
    do
    {
        uint32_t n = std::min<uint32_t>((uint32_t)P2P_CHUNK, size - done);
        Chunk c;
        c.seq = l.sendSeq++;
        c.data.assign(p + done, n);
        done += n;
        c.flags = done == size ? CHUNK_LAST : 0;
        l.waiting.push_back(std::move(c));
    } while (done < size);
    flush(l, seconds());
    return true;
}

void LarvaOnline::relay(Link &l, std::deque<Chunk> &chunks)
{
    std::string batch;
    for (const Chunk &c : chunks)
    {
        putChunk(batch, c.seq, c.flags, c.data);
        if (batch.size() >= RELAY_BATCH)
        {
            l.relayBytes += batch.size();
            sendBody(dataBody(l.peer, batch));
            batch.clear();
        }
    }
    if (!batch.empty())
    {
        l.relayBytes += batch.size();
        sendBody(dataBody(l.peer, batch));
    }
}

// Le direct ne passe plus : tout ce qui n'a pas été confirmé repart par le relais (doublons écartés à l'arrivée)
void LarvaOnline::fallback(Link &l, double now)
{
    l.direct = false;
    l.fallbacks++;
    std::deque<Chunk> pending;
    for (Chunk &c : l.unacked)
        pending.push_back(std::move(c));
    for (Chunk &c : l.waiting)
        pending.push_back(std::move(c));
    l.unacked.clear();
    l.waiting.clear();
    relay(l, pending);
    l.nextPunch = now + PUNCH_RETRY;
    l.punchUntil = 0;
}

void LarvaOnline::sendPacket(Link &l, const uint8_t *addr, uint8_t type, const std::string &chunks, double now)
{
    std::string plain;
    plain += (char)type;
    putU32(plain, l.recvNext);
    uint64_t sack = 0;
    for (int i = 0; i < 64; ++i)
        if (l.recvBuf.count(l.recvNext + 1 + i))
            sack |= 1ull << i;
    putU64(plain, sack);
    plain += chunks;
    std::string header;
    header += (char)P2P_MAGIC;
    putU32(header, l.pair);
    putU64(header, l.sendNonce);
    std::string packet = header;
    packet.resize(P2P_HEADER + plain.size() + larva::crypto::TAG);
    larva::crypto::seal(l.sendKey, l.sendNonce++, header.data(), P2P_HEADER, plain.data(), plain.size(), (uint8_t *)&packet[P2P_HEADER]);
    l.lastSend = now;
    l.ackDue = false;
    if (blockDirect || udpSock == (uintptr_t)NO_SOCKET)
        return;
    sendto((SOCKET)udpSock, packet.data(), (int)packet.size(), 0, (const sockaddr *)addr, 16);
    if (!chunks.empty())
        l.directBytes += chunks.size();
}

void LarvaOnline::sendChunksDirect(Link &l, std::vector<Chunk *> &list, double now)
{
    const size_t room = P2P_MAX - P2P_HEADER - larva::crypto::TAG - 13;
    std::string payload;
    for (Chunk *c : list)
    {
        if (!payload.empty() && payload.size() + CHUNK_HEADER + c->data.size() > room)
        {
            sendPacket(l, l.addr, P2P_DATA, payload, now);
            payload.clear();
        }
        putChunk(payload, c->seq, c->flags, c->data);
        if (c->tries == 0)
            c->firstSent = now;
        c->sentAt = now;
        c->tries++;
    }
    if (!payload.empty())
        sendPacket(l, l.addr, P2P_DATA, payload, now);
}

void LarvaOnline::flush(Link &l, double now)
{
    std::vector<Chunk *> list;
    // Renvois : délai d'attente doublé à chaque essai, au plus x4 (les pertes isolées ne doivent pas tout ralentir)
    double rto = l.srtt > 0 ? std::max(0.08, l.srtt * 2 + 0.03) : 0.25;
    for (Chunk &c : l.unacked)
        if (now - c.sentAt > rto * (1 << std::min(c.tries - 1, 2)))
            list.push_back(&c);
    size_t fresh = 0;
    while (!l.waiting.empty() && l.unacked.size() < WINDOW && fresh < BURST)
    {
        l.unacked.push_back(std::move(l.waiting.front()));
        l.waiting.pop_front();
        list.push_back(&l.unacked.back());
        fresh++;
    }
    if (!list.empty())
        sendChunksDirect(l, list, now);
    else if (l.ackDue || now - l.lastSend > KEEPALIVE)
        sendPacket(l, l.addr, P2P_DATA, "", now);
}

void LarvaOnline::tickLinks(double now)
{
    const bool canDirect = cfg.allowDirect && !blockDirect && udpSock != (uintptr_t)NO_SOCKET;
    for (auto &e : links)
    {
        Link &l = e.second;
        if (l.direct && (now - l.lastRecv > DEAD_SILENCE || (!l.unacked.empty() && now - l.unacked.front().firstSent > UNACKED_LIMIT)))
            fallback(l, now);
        if (l.direct)
        {
            flush(l, now);
            continue;
        }
        // Traversée des box : les deux joueurs s'envoient des sondes en même temps, chacun ouvre ainsi sa box à l'autre
        if (!canDirect || (!l.hasPub && !l.hasLocal))
            continue;
        if (now >= l.nextPunch)
        {
            l.punchUntil = now + PUNCH_TIME;
            l.nextPunch = now + PUNCH_TIME + PUNCH_RETRY;
            l.nextProbe = now;
        }
        if (now < l.punchUntil && now >= l.nextProbe)
        {
            l.nextProbe = now + PROBE_EVERY;
            if (l.hasPub)
                sendPacket(l, l.pubAddr, P2P_PROBE, "", now);
            if (l.hasLocal && (!l.hasPub || !sameAddr(l.pubAddr, l.localAddr)))
                sendPacket(l, l.localAddr, P2P_PROBE, "", now);
        }
    }
}

void LarvaOnline::handleUdp(const uint8_t *from, const std::string &packet, double now)
{
    if (blockDirect || !cfg.allowDirect)
        return;
    if (lossRate > 0)
    {
        static std::mt19937 rng(1234);
        if (std::uniform_real_distribution<float>(0, 1)(rng) < lossRate)
            return;
    }
    if (packet.size() < P2P_HEADER + 13 + larva::crypto::TAG || (uint8_t)packet[0] != P2P_MAGIC)
        return;
    auto pit = pairs.find(getU32(packet.data() + 1));
    if (pit == pairs.end())
        return;
    auto lit = links.find(pit->second);
    if (lit == links.end())
        return;
    Link &l = lit->second;
    uint64_t nonce = getU64(packet.data() + 5);
    // Anti-rejeu : fenêtre glissante des 64 derniers nonces
    if (l.anyNonce && (nonce + 64 <= l.recvMax || (nonce <= l.recvMax && (l.recvWindow >> (l.recvMax - nonce)) & 1)))
        return;
    std::string plain(packet.size() - P2P_HEADER - larva::crypto::TAG, '\0');
    if (!larva::crypto::open(l.recvKey, nonce, packet.data(), P2P_HEADER, packet.data() + P2P_HEADER, packet.size() - P2P_HEADER,
                             (uint8_t *)&plain[0]))
        return;
    if (!l.anyNonce || nonce > l.recvMax)
    {
        uint64_t shift = l.anyNonce ? nonce - l.recvMax : 64;
        l.recvWindow = shift >= 64 ? 1 : (l.recvWindow << shift) | 1;
        l.recvMax = nonce;
        l.anyNonce = true;
    }
    else
        l.recvWindow |= 1ull << (l.recvMax - nonce);
    l.lastRecv = now;

    uint8_t type = (uint8_t)plain[0];
    if (type == P2P_PROBE)
    {
        // Le chemin vers nous passe : on répond à l'adresse d'où vient la sonde
        sendPacket(l, from, P2P_PROBE_ACK, "", now);
        return;
    }
    // Une réponse à nos sondes, ou des données : le chemin passe dans les deux sens
    if (!l.direct)
    {
        l.direct = true;
        std::memcpy(l.addr, from, 16);
        l.ackDue = true;
    }
    else if (!sameAddr(l.addr, from))
        std::memcpy(l.addr, from, 16); // la box de l'autre a changé de port

    // Accusés : tout ce qui précède "accusé", plus les morceaux marqués dans "reçus"
    uint32_t ackNext = getU32(plain.data() + 1);
    uint64_t sack = getU64(plain.data() + 5);
    if (!l.unacked.empty())
    {
        std::deque<Chunk> keep;
        for (Chunk &c : l.unacked)
        {
            uint32_t d = c.seq - ackNext - 1;
            bool acked = before(c.seq, ackNext) || (d < 64 && ((sack >> d) & 1));
            if (!acked)
                keep.push_back(std::move(c));
            else if (c.tries == 1)
            {
                double sample = now - c.sentAt;
                l.srtt = l.srtt > 0 ? l.srtt * 0.875 + sample * 0.125 : sample;
            }
        }
        l.unacked.swap(keep);
    }
    if (type == P2P_DATA && plain.size() > 13)
    {
        l.ackDue = true;
        readChunks(l, plain.data() + 13, plain.size() - 13); // dernier usage de l : onMessage peut quitter le salon
    }
}

bool LarvaOnline::readChunks(Link &l, const char *p, size_t n)
{
    Online::UserId peer = l.peer;
    while (n >= CHUNK_HEADER)
    {
        uint32_t seq = getU32(p);
        uint8_t flags = (uint8_t)p[4];
        uint32_t len = getU32(p + 5);
        if (len > n - CHUNK_HEADER)
            return false;
        receiveChunk(l, seq, flags, std::string(p + CHUNK_HEADER, len));
        p += CHUNK_HEADER + len;
        n -= CHUNK_HEADER + len;
        // onMessage peut avoir quitté le salon (liaison supprimée) : on s'arrête là
        if (!links.count(peer))
            return false;
    }
    return n == 0;
}

void LarvaOnline::receiveChunk(Link &l, uint32_t seq, uint8_t flags, std::string data)
{
    if (before(seq, l.recvNext) || seq - l.recvNext > 16384 || l.recvBuf.count(seq))
        return; // doublon (renvoi, ou direct + relais) ou trop loin
    Chunk c;
    c.seq = seq;
    c.flags = flags;
    c.data = std::move(data);
    l.recvBuf.emplace(seq, std::move(c));
    // Messages complets, dans l'ordre
    std::vector<std::string> ready;
    for (auto it = l.recvBuf.find(l.recvNext); it != l.recvBuf.end(); it = l.recvBuf.find(l.recvNext))
    {
        l.partial += it->second.data;
        if (it->second.flags & CHUNK_LAST)
        {
            ready.push_back(std::move(l.partial));
            l.partial.clear();
        }
        l.recvBuf.erase(it);
        l.recvNext++;
    }
    Online::UserId peer = l.peer;
    for (const std::string &msg : ready)
    {
        if (!members.count(peer) || !onMessage)
            break;
        onMessage(peer, msg); // (peut modifier le salon : l n'est plus utilisé ensuite)
    }
}

// ---------------------------------------------------------------------------
//  Session partagée par le launcher et les jeux
// ---------------------------------------------------------------------------

std::string LarvaOnline::sessionFolder() const
{
    std::string dir = cfg.sessionDir;
    const char *home = std::getenv("LARVA_ONLINE_HOME");
    const char *appdata = std::getenv("APPDATA");
    if (dir.empty())
        dir = home ? home : appdata ? std::string(appdata) + "\\LarvaOnline" : "LarvaOnline";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

// Une ligne par serveur : "serveur port pseudo jeton"
void LarvaOnline::loadSession(std::string &name, std::string &token) const
{
    std::ifstream f(sessionFolder() + "\\session.txt");
    std::string server, n, t;
    int port = 0;
    while (f >> server >> port >> n >> t)
        if (server == cfg.server && port == cfg.port)
        {
            name = n;
            token = t;
        }
}

void LarvaOnline::saveSession(const std::string &name, const std::string &token) const
{
    std::string path = sessionFolder() + "\\session.txt";
    std::vector<std::string> keep;
    {
        std::ifstream f(path);
        std::string server, n, t;
        int port = 0;
        while (f >> server >> port >> n >> t)
            if (server != cfg.server || port != cfg.port)
                keep.push_back(server + " " + std::to_string(port) + " " + n + " " + t);
    }
    if (!name.empty())
        keep.push_back(cfg.server + " " + std::to_string(cfg.port) + " " + name + " " + token);
    std::ofstream f(path, std::ios::trunc);
    for (const auto &line : keep)
        f << line << "\n";
}

// ---------------------------------------------------------------------------
//  Compte, amis, salons
// ---------------------------------------------------------------------------

void LarvaOnline::login(const std::string &name, const std::string &password, bool remember)
{
    if (available())
    {
        error = "Deja connecte (deconnecte-toi d'abord)";
        return;
    }
    if (!connected)
    {
        error = "Serveur Larva Online injoignable";
        return;
    }
    rememberLogin = remember;
    error.clear();
    state_ = State::LoggingIn;
    command({"LOGIN", name, secretOf(name, password)});
}

void LarvaOnline::createAccount(const std::string &name, const std::string &password, bool remember)
{
    if (available())
    {
        error = "Deja connecte (deconnecte-toi d'abord)";
        return;
    }
    if (!connected)
    {
        error = "Serveur Larva Online injoignable";
        return;
    }
    if (!validName(name))
    {
        error = "Pseudo invalide (3 a 16 lettres, chiffres, _ ou -)";
        return;
    }
    if (password.size() < 6)
    {
        error = "Mot de passe trop court (6 caracteres minimum)";
        return;
    }
    rememberLogin = remember;
    error.clear();
    state_ = State::LoggingIn;
    command({"REGISTER", name, secretOf(name, password)});
}

void LarvaOnline::logout()
{
    std::string name, token;
    loadSession(name, token);
    command({"LOGOUT", token});
    saveSession("", "");
    resetLobby(true);
    clearLinks();
    friendList.clear();
    pendingInvites.clear();
    me = 0;
    myName_.clear();
    if (state_ != State::Offline && state_ != State::Connecting)
        state_ = State::NeedLogin;
}

std::string LarvaOnline::userName(Online::UserId id)
{
    auto it = names.find(id);
    return it == names.end() ? "" : it->second;
}

std::vector<Online::Friend> LarvaOnline::friends()
{
    std::vector<Online::Friend> out;
    for (const auto &e : friendList)
        out.push_back(e.second);
    return out;
}

bool LarvaOnline::requestTicket(const std::string &audience)
{
    if (!available() || audience.empty())
        return false;
    command({"TICKET", OnlineProto::clean(audience)});
    return true;
}

void LarvaOnline::addFriend(const std::string &name)
{
    if (available() && !name.empty())
        command({"FRIEND_ADD", name});
}

void LarvaOnline::acceptFriend(Online::UserId id)
{
    if (available())
        command({"FRIEND_ACCEPT", std::to_string(id)});
}

void LarvaOnline::removeFriend(Online::UserId id)
{
    if (available())
        command({"FRIEND_REMOVE", std::to_string(id)});
}

bool LarvaOnline::inviteFriend(Online::UserId id)
{
    if (!available() || !lobby)
        return false;
    command({"INVITE", std::to_string(id)});
    return true;
}

bool LarvaOnline::joinFriend(Online::UserId id)
{
    auto it = friendList.find(id);
    if (it == friendList.end() || !it->second.lobby || it->second.game != cfg.game)
        return false;
    if (onJoinRequested)
        onJoinRequested(it->second.lobby);
    else
        joinLobby(it->second.lobby);
    return true;
}

void LarvaOnline::acceptInvite(size_t index)
{
    if (index >= pendingInvites.size())
        return;
    Online::Invite inv = pendingInvites[index];
    pendingInvites.erase(pendingInvites.begin() + index);
    if (inv.game != cfg.game)
        return;
    if (onJoinRequested)
        onJoinRequested(inv.lobby);
    else
        joinLobby(inv.lobby);
}

void LarvaOnline::dismissInvite(size_t index)
{
    if (index < pendingInvites.size())
        pendingInvites.erase(pendingInvites.begin() + index);
}

void LarvaOnline::createLobby(int maxMembers)
{
    createLobby(maxMembers, false);
}

void LarvaOnline::createLobby(int maxMembers, bool hostMigration)
{
    if (!available())
    {
        error = "Connecte-toi a Larva Online pour heberger";
        if (onLobbyEntered)
            onLobbyEntered(0, false);
        return;
    }
    joiningLobby = true;
    command({"LOBBY_CREATE", std::to_string(maxMembers), hostMigration ? "1" : "0"});
}

void LarvaOnline::joinLobby(Online::LobbyId id)
{
    // Pas encore connecté (jeu lancé par une invitation) : on rejoindra une fois le compte connecté
    if (!available() && state_ != State::Offline)
    {
        deferredJoin = id;
        return;
    }
    if (!available())
    {
        error = "Connecte-toi a Larva Online pour rejoindre";
        if (onLobbyEntered)
            onLobbyEntered(0, false);
        return;
    }
    joiningLobby = true;
    command({"LOBBY_JOIN", std::to_string(id)});
}

void LarvaOnline::leaveLobby()
{
    if (lobby)
        command({"LOBBY_LEAVE"});
    resetLobby(false);
    clearLinks();
}

Online::LobbyId LarvaOnline::lobbyFromCommandLine()
{
    std::string cmd = GetCommandLineA();
    size_t pos = cmd.find("+larva_lobby");
    if (pos == std::string::npos)
        return 0;
    return std::strtoull(cmd.c_str() + pos + 12, nullptr, 10);
}
