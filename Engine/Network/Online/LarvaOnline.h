#pragma once
#ifndef __LARVA_ONLINE__
#define __LARVA_ONLINE__
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include "OnlineService.h"
#include "SecureChannel.h"

// Client Larva Online (protocole : OnlineProtocol.h). Une connexion TCP chiffrée vers le serveur, ouverte et rouverte
// par un thread ; une socket UDP pour les connexions directes entre joueurs. Tout l'état (compte, amis, salon,
// liaisons) vit sur le thread principal et change dans update().
//
// Données entre membres d'un salon : chaque message est découpé en morceaux numérotés. En direct (UDP chiffré, avec
// accusés de réception et renvois) quand les deux box le permettent ; sinon, ou si le direct tombe, par le relais du
// serveur. Le joueur reçoit toujours ses messages entiers, dans l'ordre, sans perte.
//
// La session ("rester connecté") est partagée par le launcher et tous les jeux :
// %APPDATA%\LarvaOnline\session.txt (dossier remplaçable par Config::sessionDir ou LARVA_ONLINE_HOME).
class LarvaOnline : public Online::Service
{
public:
    enum class State
    {
        Connecting, // connexion au serveur en cours
        Offline,    // serveur injoignable (nouvel essai toutes les 5 s)
        NeedLogin,  // connecté au serveur, aucun compte
        LoggingIn,  // identifiants envoyés
        Ready       // compte connecté
    };

    explicit LarvaOnline(const Online::Config &config);
    ~LarvaOnline() override;

    Online::Backend backend() const override { return Online::Backend::Larva; }
    const char *displayName() const override { return "Larva Online"; }
    bool available() override { return state_ == State::Ready; }
    void update() override;
    void shutdown() override;

    Online::UserId myId() override { return me; }
    std::string myName() override { return myName_; }
    std::string userName(Online::UserId id) override;

    void createLobby(int maxMembers) override;
    void createLobby(int maxMembers, bool hostMigration) override;
    void joinLobby(Online::LobbyId lobby) override;
    void leaveLobby() override;
    Online::LobbyId currentLobby() override { return lobby; }
    Online::UserId lobbyOwner() override { return owner; }
    int lobbyMemberCount() override { return (int)members.size(); }
    bool lobbyHasMember(Online::UserId id) override { return members.count(id) != 0; }
    std::vector<Online::UserId> lobbyMembers() override
    {
        std::vector<Online::UserId> out;
        for (const auto &m : members)
            out.push_back(m.first);
        return out;
    }
    void openInviteOverlay() override { overlayRequested = true; }
    Online::LobbyId lobbyFromCommandLine() override;

    std::vector<Online::Friend> friends() override;
    bool inviteFriend(Online::UserId id) override;
    bool directConnection(Online::UserId id) override;

    bool send(Online::UserId to, const void *data, uint32_t size) override;
    using Online::Service::send;
    void closeSession(Online::UserId) override {}

    // --- Compte ---
    State state() const { return state_; }
    // Dernière erreur du serveur (identifiants, ami introuvable, clé du serveur...), effacée par clearError()
    const std::string &lastError() const { return error; }
    void clearError() { error.clear(); }
    void login(const std::string &name, const std::string &password, bool remember = true);
    void createAccount(const std::string &name, const std::string &password, bool remember = true);
    // Déconnecte le compte et oublie la session enregistrée (launcher et jeux)
    void logout();

    // --- Amis ---
    void addFriend(const std::string &name);
    void acceptFriend(Online::UserId id);
    void removeFriend(Online::UserId id); // retire l'ami, ou refuse / annule une demande
    // Rejoindre la partie d'un ami (même jeu, salon joignable) : passe par onJoinRequested comme une invitation
    bool joinFriend(Online::UserId id);

    // --- Serveurs de jeu dédiés ---
    // Ticket prouvant le compte à un serveur de jeu (voir OnlineProtocol.h) ; audience donnée par ce serveur.
    // Réponse dans onTicket (ticket vide et raison en cas d'échec). false : pas connecté à un compte.
    bool requestTicket(const std::string &audience);
    std::function<void(const std::string &audience, const std::string &ticket, const std::string &error)> onTicket;

    // --- Invitations reçues ---
    const std::vector<Online::Invite> &invites() const { return pendingInvites; }
    void acceptInvite(size_t index); // onJoinRequested(salon) si l'invitation est pour ce jeu
    void dismissInvite(size_t index);

    const Online::Config &config() const { return cfg; }
    const std::string &game() const { return cfg.game; }
    // Le jeu a demandé le panneau d'amis (openInviteOverlay) : lu et remis à zéro par OnlineOverlay
    bool takeOverlayRequest()
    {
        bool r = overlayRequested;
        overlayRequested = false;
        return r;
    }

    // --- Liaisons avec les membres du salon (diagnostic, tests) ---
    struct LinkStats
    {
        bool exists = false, direct = false;
        double rtt = 0;                         // aller-retour en direct (s)
        uint64_t directBytes = 0, relayBytes = 0; // données envoyées par chaque chemin
        int fallbacks = 0;                      // passages du direct au relais
    };
    LinkStats linkStats(Online::UserId peer) const;
    // UDP vu par le serveur (adresse publique connue : le direct est possible)
    bool udpReady() const { return udpSeen; }
    // Tests : perte d'une part des paquets UDP reçus, ou coupure totale du direct (dans les deux sens)
    void debugUdpLoss(float rate) { lossRate = rate; }
    void debugBlockDirect(bool blocked);

private:
    Online::Config cfg;
    State state_ = State::Connecting;
    std::string error;
    Online::UserId me = 0;
    std::string myName_;
    bool rememberLogin = true;
    std::map<Online::UserId, Online::Friend> friendList;
    std::map<Online::UserId, std::string> names; // pseudos connus (amis, membres de salon)
    std::vector<Online::Invite> pendingInvites;
    bool overlayRequested = false;

    Online::LobbyId lobby = 0;
    Online::UserId owner = 0;
    std::map<Online::UserId, std::string> members;
    bool joiningLobby = false;
    Online::LobbyId deferredJoin = 0; // salon à rejoindre dès que le compte sera connecté

    // --- Liaisons directes ---
    struct Chunk
    {
        uint32_t seq = 0;
        uint8_t flags = 0;
        std::string data;
        double sentAt = 0, firstSent = 0;
        int tries = 0;
    };
    struct Link
    {
        Online::UserId peer = 0;
        uint32_t pair = 0;
        uint8_t sendKey[32], recvKey[32];
        uint64_t sendNonce = 0, recvMax = 0, recvWindow = 0;
        bool anyNonce = false;
        uint8_t pubAddr[16], localAddr[16]; // sockaddr_in
        bool hasPub = false, hasLocal = false;
        bool direct = false;
        uint8_t addr[16];
        double lastRecv = 0, lastSend = 0, punchUntil = 0, nextProbe = 0, nextPunch = 0;
        uint32_t sendSeq = 0;
        std::deque<Chunk> unacked, waiting;
        uint32_t recvNext = 0;
        std::map<uint32_t, Chunk> recvBuf;
        std::string partial;
        bool ackDue = false;
        double srtt = 0;
        uint64_t directBytes = 0, relayBytes = 0;
        int fallbacks = 0;
    };
    std::map<Online::UserId, Link> links;
    std::map<uint32_t, Online::UserId> pairs;
    uintptr_t udpSock;
    int udpPort = 0;
    std::string localIp = "-";
    uint8_t serverUdpAddr[16];
    bool serverUdpKnown = false;
    std::string udpToken;
    bool udpSeen = false;
    double nextUdpHello = 0;
    float lossRate = 0;
    bool blockDirect = false;

    void onPeerInfo(const std::vector<std::string> &f);
    void setCandidates(Link &l, const std::string &pubIp, const std::string &pubPort, const std::string &lanIp, const std::string &lanPort);
    void dropLink(Online::UserId peer);
    void clearLinks();
    void tickLinks(double now);
    void flush(Link &l, double now);
    void sendPacket(Link &l, const uint8_t *addr, uint8_t type, const std::string &chunks, double now);
    void sendChunksDirect(Link &l, std::vector<Chunk *> &list, double now);
    void relay(Link &l, std::deque<Chunk> &chunks);
    void fallback(Link &l, double now);
    void handleUdp(const uint8_t *from, const std::string &packet, double now);
    bool readChunks(Link &l, const char *p, size_t n);
    void receiveChunk(Link &l, uint32_t seq, uint8_t flags, std::string data);

    // --- Thread réseau ---
    struct Event
    {
        enum Kind
        {
            Connected,
            Disconnected,
            Command,
            Data,
            Udp,
            Fatal
        } kind;
        std::vector<std::string> fields;
        Online::UserId peer = 0;
        std::string data;
        uint8_t addr[16];
    };
    std::thread worker, udpWorker;
    std::atomic<bool> stopping{false};
    std::mutex eventsMutex;
    std::deque<Event> events;
    std::mutex sendMutex;
    std::atomic<uintptr_t> sock;
    std::atomic<bool> connected{false};
    SecureChannel channel; // envoi protégé par sendMutex, réception par le thread réseau seul

    void run();
    void runUdp();
    bool handshake(uintptr_t s, SecureChannel &ch, std::string &fatal);
    void push(Event e);
    bool sendBody(const std::string &body);
    void command(const std::vector<std::string> &fields);
    void handle(const std::vector<std::string> &f);
    void resetLobby(bool notify);
    void loadSession(std::string &name, std::string &token) const;
    void saveSession(const std::string &name, const std::string &token) const;
    std::string sessionFolder() const;
};

namespace Online
{
    // Service Larva Online courant (nullptr si le jeu passe par Steam)
    LarvaOnline *larva();
}

#endif
