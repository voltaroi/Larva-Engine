#pragma once
#ifndef __STEAM_NET__
#define __STEAM_NET__
#include <cstdint>
#include <functional>
#include <string>

// Multijoueur par Steam : salons (lobbies), invitations d'amis et échange de données entre joueurs
// par le réseau de Steam (traversée des box et relais de Valve : ni IP, ni ports, ni VPN à configurer).
//
// Compilé avec LARVA_STEAM défini et le SDK Steamworks (Dependencies/steamworks) ; sinon toutes les
// fonctions sont des coquilles vides et available() renvoie false.
//
// Utilisation :
//   SteamNet::init(480);                          // App ID du jeu (480 : application de test de Valve)
//   SteamNet::onMessage = [](uint64_t from, const std::string &data) { ... };
//   chaque image : SteamNet::update();
//   héberger : SteamNet::createLobby(8);  puis SteamNet::openInviteOverlay();
//   rejoindre : automatique quand le joueur accepte une invitation (onJoinRequested -> joinLobby)
namespace SteamNet
{
    // Démarre l'API Steam (Steam doit être lancé). Écrit steam_appid.txt à côté du jeu pour les lancements hors Steam.
    bool init(uint32_t appId, std::string *error = nullptr);
    void shutdown();
    bool available();
    // Callbacks Steam et messages reçus : à appeler à chaque image
    void update();

    uint64_t mySteamId();
    std::string personaName();
    std::string friendName(uint64_t steamId);

    // --- Salons ---
    // Salon réservé aux amis ; onLobbyEntered est appelé une fois créé (le créateur en est le propriétaire)
    void createLobby(int maxMembers);
    void joinLobby(uint64_t lobbyId);
    void leaveLobby();
    uint64_t currentLobby();
    uint64_t lobbyOwner();
    int lobbyMemberCount();
    // Fenêtre Steam (Maj+Tab) pour inviter des amis dans le salon
    void openInviteOverlay();
    // Salon passé en ligne de commande par Steam ("+connect_lobby <id>") quand le jeu est lancé par une invitation
    uint64_t lobbyFromCommandLine();

    // --- Données ---
    // Envoi fiable et ordonné à un joueur (crée la session au premier envoi)
    bool send(uint64_t steamId, const void *data, uint32_t size);
    bool send(uint64_t steamId, const std::string &data);
    void closeSession(uint64_t steamId);

    // --- Événements (appelés depuis update(), sur le thread principal) ---
    extern std::function<void(uint64_t lobby, bool ok)> onLobbyEntered;
    extern std::function<void(uint64_t lobby)> onJoinRequested;       // invitation acceptée / "Rejoindre la partie"
    extern std::function<void(uint64_t member)> onMemberJoined;
    extern std::function<void(uint64_t member)> onMemberLeft;
    extern std::function<void(uint64_t from, const std::string &data)> onMessage;
    extern std::function<void(uint64_t peer)> onSessionFailed;        // joueur injoignable
    // Filtre des demandes de session entrantes (par défaut : membres du salon courant)
    extern std::function<bool(uint64_t peer)> acceptSession;
}

#endif
