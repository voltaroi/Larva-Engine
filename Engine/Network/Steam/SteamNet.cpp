#include "SteamNet.h"

namespace SteamNet
{
    std::function<void(uint64_t, bool)> onLobbyEntered;
    std::function<void(uint64_t)> onJoinRequested;
    std::function<void(uint64_t)> onMemberJoined;
    std::function<void(uint64_t)> onMemberLeft;
    std::function<void(uint64_t, const std::string &)> onMessage;
    std::function<void(uint64_t)> onSessionFailed;
    std::function<bool(uint64_t)> acceptSession;
}

#ifdef LARVA_STEAM
// API "plate" en C de Steamworks : pas de dépendance à l'ABI C++ de MSVC (compilable avec MinGW / clang),
// callbacks lus à la main (SteamAPI_ManualDispatch)
#include <steam/steam_api.h>
#include <steam/steam_api_flat.h>
#include <steam/isteamnetworkingmessages.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace SteamNet
{
    namespace
    {
        bool ready = false;
        uint64_t lobby = 0;
        const int CHANNEL = 0;

        SteamNetworkingIdentity identityOf(uint64_t steamId)
        {
            SteamNetworkingIdentity id;
            id.Clear();
            SteamAPI_SteamNetworkingIdentity_SetSteamID64(&id, steamId);
            return id;
        }

        bool inLobby(uint64_t steamId)
        {
            if (!lobby)
                return false;
            ISteamMatchmaking *mm = SteamAPI_SteamMatchmaking_v009();
            int n = SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(mm, lobby);
            for (int i = 0; i < n; ++i)
                if (SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(mm, lobby, i) == steamId)
                    return true;
            return false;
        }

        void enteredLobby(uint64_t id, bool ok)
        {
            if (ok)
            {
                lobby = id;
                // "Rejoindre la partie" dans la liste d'amis Steam : Steam relance le jeu avec cette commande
                char connect[64];
                std::snprintf(connect, sizeof(connect), "+connect_lobby %llu", (unsigned long long)id);
                SteamAPI_ISteamFriends_SetRichPresence(SteamAPI_SteamFriends_v018(), "connect", connect);
            }
            if (onLobbyEntered)
                onLobbyEntered(id, ok);
        }

        void handleCallback(int type, const void *data)
        {
            switch (type)
            {
            case GameLobbyJoinRequested_t::k_iCallback:
            {
                const GameLobbyJoinRequested_t *e = (const GameLobbyJoinRequested_t *)data;
                if (onJoinRequested)
                    onJoinRequested(e->m_steamIDLobby.ConvertToUint64());
                break;
            }
            case LobbyEnter_t::k_iCallback:
            {
                const LobbyEnter_t *e = (const LobbyEnter_t *)data;
                enteredLobby(e->m_ulSteamIDLobby, e->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess);
                break;
            }
            case LobbyChatUpdate_t::k_iCallback:
            {
                const LobbyChatUpdate_t *e = (const LobbyChatUpdate_t *)data;
                if (e->m_ulSteamIDLobby != lobby)
                    break;
                if (e->m_rgfChatMemberStateChange & k_EChatMemberStateChangeEntered)
                {
                    if (onMemberJoined)
                        onMemberJoined(e->m_ulSteamIDUserChanged);
                }
                else if (onMemberLeft)
                    onMemberLeft(e->m_ulSteamIDUserChanged);
                break;
            }
            case SteamNetworkingMessagesSessionRequest_t::k_iCallback:
            {
                const SteamNetworkingMessagesSessionRequest_t *e = (const SteamNetworkingMessagesSessionRequest_t *)data;
                SteamNetworkingIdentity id = e->m_identityRemote;
                uint64_t peer = SteamAPI_SteamNetworkingIdentity_GetSteamID64(&id);
                bool accept = acceptSession ? acceptSession(peer) : inLobby(peer);
                if (accept)
                    SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser(SteamAPI_SteamNetworkingMessages_SteamAPI_v002(), id);
                break;
            }
            case SteamNetworkingMessagesSessionFailed_t::k_iCallback:
            {
                const SteamNetworkingMessagesSessionFailed_t *e = (const SteamNetworkingMessagesSessionFailed_t *)data;
                SteamNetworkingIdentity id = e->m_info.m_identityRemote;
                if (onSessionFailed)
                    onSessionFailed(SteamAPI_SteamNetworkingIdentity_GetSteamID64(&id));
                break;
            }
            }
        }
    }

    bool init(uint32_t appId, std::string *error)
    {
        if (ready)
            return true;
        // Lancement hors de Steam (développement) : l'App ID est lu dans steam_appid.txt ou la variable SteamAppId
        char appText[16];
        std::snprintf(appText, sizeof(appText), "%u", appId);
        SetEnvironmentVariableA("SteamAppId", appText);
        SetEnvironmentVariableA("SteamGameId", appText);
        char path[MAX_PATH] = {0};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string file(path);
        size_t slash = file.find_last_of("\\/");
        file = (slash == std::string::npos ? std::string(".") : file.substr(0, slash)) + "\\steam_appid.txt";
        if (FILE *f = std::fopen(file.c_str(), "w"))
        {
            std::fputs(appText, f);
            std::fclose(f);
        }

        SteamErrMsg message = {0};
        if (SteamAPI_InitFlat(&message) != k_ESteamAPIInitResult_OK)
        {
            if (error)
                *error = message[0] ? message : "Steam n'est pas lance";
            return false;
        }
        // Lecture manuelle des callbacks : à activer après l'initialisation (sinon aucun callback n'arrive)
        SteamAPI_ManualDispatch_Init();
        ready = true;
        // Prépare tout de suite l'accès aux relais de Valve (première connexion plus rapide)
        SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(SteamAPI_SteamNetworkingUtils_SteamAPI_v004());
        std::atexit(shutdown);
        return true;
    }

    void shutdown()
    {
        if (!ready)
            return;
        leaveLobby();
        SteamAPI_Shutdown();
        ready = false;
    }

    bool available() { return ready; }

    void update()
    {
        if (!ready)
            return;
        HSteamPipe pipe = SteamAPI_GetHSteamPipe();
        SteamAPI_ManualDispatch_RunFrame(pipe);
        CallbackMsg_t msg;
        while (SteamAPI_ManualDispatch_GetNextCallback(pipe, &msg))
        {
#ifdef STEAMNET_DEBUG
            std::printf("[steam] callback %d (%d octets)\n", msg.m_iCallback, msg.m_cubParam);
#endif
            if (msg.m_iCallback == SteamAPICallCompleted_t::k_iCallback)
            {
                // Résultat d'un appel asynchrone (création de salon...)
                const SteamAPICallCompleted_t *done = (const SteamAPICallCompleted_t *)msg.m_pubParam;
                std::vector<unsigned char> result(done->m_cubParam);
                bool failed = false;
                if (SteamAPI_ManualDispatch_GetAPICallResult(pipe, done->m_hAsyncCall, result.data(), done->m_cubParam,
                                                             done->m_iCallback, &failed))
                {
                    if (done->m_iCallback == LobbyCreated_t::k_iCallback)
                    {
                        const LobbyCreated_t *created = (const LobbyCreated_t *)result.data();
                        if (failed || created->m_eResult != k_EResultOK)
                            enteredLobby(0, false);
                        // Sinon LobbyEnter_t suit et termine l'entrée dans le salon
                    }
                    else if (done->m_iCallback == LobbyEnter_t::k_iCallback && failed)
                        enteredLobby(0, false);
                }
            }
            else
                handleCallback(msg.m_iCallback, msg.m_pubParam);
            SteamAPI_ManualDispatch_FreeLastCallback(pipe);
        }

        // Messages reçus des autres joueurs
        ISteamNetworkingMessages *net = SteamAPI_SteamNetworkingMessages_SteamAPI_v002();
        SteamNetworkingMessage_t *messages[32];
        int count;
        while ((count = SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel(net, CHANNEL, messages, 32)) > 0)
        {
            for (int i = 0; i < count; ++i)
            {
                SteamNetworkingMessage_t *m = messages[i];
                uint64_t from = SteamAPI_SteamNetworkingIdentity_GetSteamID64(&m->m_identityPeer);
                if (onMessage)
                    onMessage(from, std::string((const char *)m->m_pData, m->m_cbSize));
                SteamAPI_SteamNetworkingMessage_t_Release(m);
            }
        }
    }

    uint64_t mySteamId()
    {
        return ready ? SteamAPI_ISteamUser_GetSteamID(SteamAPI_SteamUser_v023()) : 0;
    }

    std::string personaName()
    {
        return ready ? SteamAPI_ISteamFriends_GetPersonaName(SteamAPI_SteamFriends_v018()) : "";
    }

    std::string friendName(uint64_t steamId)
    {
        return ready ? SteamAPI_ISteamFriends_GetFriendPersonaName(SteamAPI_SteamFriends_v018(), steamId) : "";
    }

    void createLobby(int maxMembers)
    {
        if (!ready)
            return;
        SteamAPICall_t call = SteamAPI_ISteamMatchmaking_CreateLobby(SteamAPI_SteamMatchmaking_v009(), k_ELobbyTypeFriendsOnly, maxMembers);
#ifdef STEAMNET_DEBUG
        std::printf("[steam] CreateLobby -> appel %llu, pipe %d, mm %p\n", (unsigned long long)call, (int)SteamAPI_GetHSteamPipe(), (void *)SteamAPI_SteamMatchmaking_v009());
#else
        (void)call;
#endif
    }

    void joinLobby(uint64_t lobbyId)
    {
        if (!ready)
            return;
        if (lobby && lobby != lobbyId)
            leaveLobby();
        SteamAPI_ISteamMatchmaking_JoinLobby(SteamAPI_SteamMatchmaking_v009(), lobbyId);
    }

    void leaveLobby()
    {
        if (!ready || !lobby)
            return;
        SteamAPI_ISteamMatchmaking_LeaveLobby(SteamAPI_SteamMatchmaking_v009(), lobby);
        SteamAPI_ISteamFriends_ClearRichPresence(SteamAPI_SteamFriends_v018());
        lobby = 0;
    }

    uint64_t currentLobby() { return lobby; }

    uint64_t lobbyOwner()
    {
        return ready && lobby ? SteamAPI_ISteamMatchmaking_GetLobbyOwner(SteamAPI_SteamMatchmaking_v009(), lobby) : 0;
    }

    int lobbyMemberCount()
    {
        return ready && lobby ? SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(SteamAPI_SteamMatchmaking_v009(), lobby) : 0;
    }

    void openInviteOverlay()
    {
        if (ready && lobby)
            SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog(SteamAPI_SteamFriends_v018(), lobby);
    }

    uint64_t lobbyFromCommandLine()
    {
        std::string cmd = GetCommandLineA();
        size_t pos = cmd.find("+connect_lobby");
        if (pos == std::string::npos)
            return 0;
        return std::strtoull(cmd.c_str() + pos + 14, nullptr, 10);
    }

    bool send(uint64_t steamId, const void *data, uint32_t size)
    {
        if (!ready)
            return false;
        SteamNetworkingIdentity id = identityOf(steamId);
        EResult r = SteamAPI_ISteamNetworkingMessages_SendMessageToUser(SteamAPI_SteamNetworkingMessages_SteamAPI_v002(), id, data,
                                                                       size, k_nSteamNetworkingSend_Reliable, CHANNEL);
        return r == k_EResultOK;
    }

    bool send(uint64_t steamId, const std::string &data)
    {
        return send(steamId, data.data(), (uint32_t)data.size());
    }

    void closeSession(uint64_t steamId)
    {
        if (!ready)
            return;
        SteamNetworkingIdentity id = identityOf(steamId);
        SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser(SteamAPI_SteamNetworkingMessages_SteamAPI_v002(), id);
    }
}

#else // Sans le SDK Steamworks : Steam indisponible

namespace SteamNet
{
    bool init(uint32_t, std::string *error)
    {
        if (error)
            *error = "Steam non inclus dans cette version";
        return false;
    }
    void shutdown() {}
    bool available() { return false; }
    void update() {}
    uint64_t mySteamId() { return 0; }
    std::string personaName() { return ""; }
    std::string friendName(uint64_t) { return ""; }
    void createLobby(int) {}
    void joinLobby(uint64_t) {}
    void leaveLobby() {}
    uint64_t currentLobby() { return 0; }
    uint64_t lobbyOwner() { return 0; }
    int lobbyMemberCount() { return 0; }
    void openInviteOverlay() {}
    uint64_t lobbyFromCommandLine() { return 0; }
    bool send(uint64_t, const void *, uint32_t) { return false; }
    bool send(uint64_t, const std::string &) { return false; }
    void closeSession(uint64_t) {}
}

#endif
