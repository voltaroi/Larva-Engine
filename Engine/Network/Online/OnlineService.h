#pragma once
#ifndef __ONLINE_SERVICE__
#define __ONLINE_SERVICE__
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Services en ligne des jeux : comptes, amis, salons, invitations et échange de données entre joueurs,
// avec la même interface que le jeu passe par Steam ou par Larva Online (serveur maison, projects/online).
// Dans les deux cas, aucune adresse IP ni aucun port à ouvrir chez les joueurs.
//
//   Online::Config cfg;
//   cfg.game = "rempart";                 // identifiant du jeu sur Larva Online
//   cfg.steamAppId = 480;                 // 0 : pas de Steam
//   cfg.server = "online.mondomaine.fr";  // serveur Larva Online
//   Online::Service &svc = Online::init(cfg);   // avant la création de la fenêtre (overlay Steam)
//   svc.onMessage = [](Online::UserId from, const std::string &data) { ... };
//   chaque image : svc.update();
//   héberger : svc.createLobby(8); puis svc.openInviteOverlay();
//   rejoindre : onJoinRequested(lobby) -> svc.joinLobby(lobby)
//
// Choix du service : "--online=steam" / "--online=larva" sur la ligne de commande (le launcher passe --online=larva),
// sinon la variable d'environnement LARVA_ONLINE, sinon Steam s'il est lancé, sinon Larva Online.
// LARVA_ONLINE_SERVER=adresse[:port] remplace l'adresse du serveur Larva Online (tests).
// Avec Larva Online, le panneau d'amis du moteur (OnlineOverlay) remplace l'overlay de Steam.
//
// Fichiers à compiler : Engine/Network/Online/*, Engine/Network/Steam/* et Engine/Core/Sha256.cpp (Engine/Core/*).
// Jeu sans Steam : définir LARVA_NO_STEAM, et Engine/Network/Steam/* n'est plus nécessaire (ni steam_api64.dll).
namespace Online
{
    using UserId = uint64_t;
    using LobbyId = uint64_t;

    enum class Backend
    {
        Auto,
        Steam,
        Larva
    };

    struct Friend
    {
        enum State
        {
            Offline,
            Online,
            RequestIn,  // il te demande en ami
            RequestOut  // demande envoyée, en attente
        };
        UserId id = 0;
        std::string name;
        State state = Offline;
        std::string game;  // jeu en cours ("launcher" : seulement le launcher ouvert)
        LobbyId lobby = 0; // partie en cours (0 : aucune)
        bool joinable = false;
    };

    struct Invite
    {
        UserId from = 0;
        std::string fromName;
        LobbyId lobby = 0;
        std::string game;
    };

    struct Config
    {
        std::string game;          // identifiant du jeu (lettres, chiffres, _ ou -)
        uint32_t steamAppId = 0;   // 0 : Steam jamais utilisé
        std::string server = "127.0.0.1";
        int port = 27020;
        Backend backend = Backend::Auto;
        std::string sessionDir;    // session Larva Online (vide : %APPDATA%\LarvaOnline, partagé avec le launcher)
        // Clé publique du serveur Larva Online (64 caractères hexadécimaux, affichée au démarrage du serveur) : la
        // connexion est refusée si le serveur ne la prouve pas. Vide : clé mémorisée à la première connexion
        // (known_servers.txt dans le dossier de session), refus si elle change ensuite.
        std::string serverKey;
        // Connexion directe entre joueurs (UDP) quand les box le permettent, sinon relais par le serveur
        bool allowDirect = true;
    };

    class Service
    {
    public:
        virtual ~Service() = default;
        virtual Backend backend() const = 0;
        virtual const char *displayName() const = 0; // "Steam", "Larva Online"
        // Prêt à créer ou rejoindre des salons (Steam lancé / connecté à un compte Larva Online)
        virtual bool available() = 0;
        // Callbacks et messages reçus : à appeler à chaque image, sur le thread principal
        virtual void update() = 0;
        virtual void shutdown() = 0;

        virtual UserId myId() = 0;
        virtual std::string myName() = 0;
        virtual std::string userName(UserId id) = 0;

        // --- Salons ---
        // Salon réservé aux amis ; onLobbyEntered est appelé une fois créé (le créateur en est le propriétaire)
        virtual void createLobby(int maxMembers) = 0;
        // Salon qui survit au départ de son propriétaire : un autre membre en devient le propriétaire
        // (onLobbyOwnerChanged) au lieu de la fermeture du salon. Steam le fait toujours.
        virtual void createLobby(int maxMembers, bool hostMigration)
        {
            (void)hostMigration;
            createLobby(maxMembers);
        }
        virtual void joinLobby(LobbyId lobby) = 0;
        virtual void leaveLobby() = 0;
        virtual LobbyId currentLobby() = 0;
        virtual UserId lobbyOwner() = 0;
        virtual int lobbyMemberCount() = 0;
        virtual bool lobbyHasMember(UserId id) = 0;
        // Membres du salon, soi compris (vide si le service ne sait pas les lister)
        virtual std::vector<UserId> lobbyMembers() { return {}; }
        // Steam : overlay d'invitation ; Larva Online : ouvre le panneau d'amis (OnlineOverlay)
        virtual void openInviteOverlay() = 0;
        // Salon passé en ligne de commande quand le jeu est lancé par une invitation (Steam ou launcher)
        virtual LobbyId lobbyFromCommandLine() = 0;

        // --- Amis (Larva Online ; avec Steam, l'overlay de Steam s'en charge) ---
        virtual std::vector<Friend> friends() { return {}; }
        virtual bool inviteFriend(UserId) { return false; }
        // Les données avec ce membre du salon passent en direct (sinon par le relais du serveur)
        virtual bool directConnection(UserId) { return false; }

        // --- Données ---
        // Envoi fiable et ordonné à un joueur du salon
        virtual bool send(UserId to, const void *data, uint32_t size) = 0;
        bool send(UserId to, const std::string &data) { return send(to, data.data(), (uint32_t)data.size()); }
        virtual void closeSession(UserId peer) = 0;

        // --- Événements (appelés depuis update(), sur le thread principal) ---
        std::function<void(LobbyId lobby, bool ok)> onLobbyEntered;
        std::function<void(LobbyId lobby)> onJoinRequested; // invitation acceptée / "Rejoindre la partie"
        std::function<void(UserId member)> onMemberJoined;
        std::function<void(UserId member)> onMemberLeft;
        // Le propriétaire du salon est parti et un autre membre l'est devenu (appelé avant onMemberLeft de l'ancien)
        std::function<void(UserId owner)> onLobbyOwnerChanged;
        std::function<void(UserId from, const std::string &data)> onMessage;
        std::function<void(UserId peer)> onSessionFailed; // joueur injoignable
        // Invitation reçue (Larva Online ; le panneau d'amis l'affiche aussi)
        std::function<void(const Invite &invite)> onInviteReceived;
    };

    // Choisit et démarre le service (voir plus haut) ; renvoie toujours un service utilisable
    Service &init(const Config &config);
    // Service courant (init doit avoir été appelé)
    Service &get();
    bool initialized();
    void shutdown();
    // "--online=..." ou LARVA_ONLINE ; Auto si rien n'est précisé
    Backend requestedBackend();
}

#endif
