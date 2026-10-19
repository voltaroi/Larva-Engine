#include "OnlineService.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#ifndef LARVA_NO_STEAM
#include "../Steam/SteamNet.h"
#endif
#include "LarvaOnline.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Online
{
    namespace
    {
#ifndef LARVA_NO_STEAM
        // Steam par SteamNet : l'overlay de Steam gère les amis et les invitations
        class SteamOnline : public Service
        {
        public:
            SteamOnline()
            {
                SteamNet::onLobbyEntered = [this](uint64_t lobby, bool ok) {
                    if (onLobbyEntered)
                        onLobbyEntered(lobby, ok);
                };
                SteamNet::onJoinRequested = [this](uint64_t lobby) {
                    if (onJoinRequested)
                        onJoinRequested(lobby);
                    else
                        SteamNet::joinLobby(lobby);
                };
                SteamNet::onMemberJoined = [this](uint64_t id) {
                    if (onMemberJoined)
                        onMemberJoined(id);
                };
                SteamNet::onMemberLeft = [this](uint64_t id) {
                    if (onMemberLeft)
                        onMemberLeft(id);
                };
                SteamNet::onMessage = [this](uint64_t from, const std::string &data) {
                    if (onMessage)
                        onMessage(from, data);
                };
                SteamNet::onSessionFailed = [this](uint64_t peer) {
                    if (onSessionFailed)
                        onSessionFailed(peer);
                };
            }
            Backend backend() const override { return Backend::Steam; }
            const char *displayName() const override { return "Steam"; }
            bool available() override { return SteamNet::available(); }
            void update() override { SteamNet::update(); }
            void shutdown() override { SteamNet::shutdown(); }
            UserId myId() override { return SteamNet::mySteamId(); }
            std::string myName() override { return SteamNet::personaName(); }
            std::string userName(UserId id) override { return SteamNet::friendName(id); }
            void createLobby(int maxMembers) override { SteamNet::createLobby(maxMembers); }
            void joinLobby(LobbyId lobby) override { SteamNet::joinLobby(lobby); }
            void leaveLobby() override { SteamNet::leaveLobby(); }
            LobbyId currentLobby() override { return SteamNet::currentLobby(); }
            UserId lobbyOwner() override { return SteamNet::lobbyOwner(); }
            int lobbyMemberCount() override { return SteamNet::lobbyMemberCount(); }
            bool lobbyHasMember(UserId id) override { return SteamNet::lobbyHasMember(id); }
            void openInviteOverlay() override { SteamNet::openInviteOverlay(); }
            LobbyId lobbyFromCommandLine() override { return SteamNet::lobbyFromCommandLine(); }
            bool send(UserId to, const void *data, uint32_t size) override { return SteamNet::send(to, data, size); }
            using Service::send;
            void closeSession(UserId peer) override { SteamNet::closeSession(peer); }
        };

#endif

        std::unique_ptr<Service> current;

        std::string lowerCase(std::string s)
        {
            for (char &c : s)
                if (c >= 'A' && c <= 'Z')
                    c = (char)(c - 'A' + 'a');
            return s;
        }
    }

    Backend requestedBackend()
    {
        std::string cmd = lowerCase(GetCommandLineA());
        if (cmd.find("--online=larva") != std::string::npos || cmd.find("+larva_lobby") != std::string::npos)
            return Backend::Larva;
        if (cmd.find("--online=steam") != std::string::npos || cmd.find("+connect_lobby") != std::string::npos)
            return Backend::Steam;
        if (const char *env = std::getenv("LARVA_ONLINE"))
        {
            std::string v = lowerCase(env);
            if (v == "larva")
                return Backend::Larva;
            if (v == "steam")
                return Backend::Steam;
        }
        return Backend::Auto;
    }

    Service &init(const Config &config)
    {
        if (current)
            return *current;
        Config cfg = config;
        // LARVA_ONLINE_SERVER=adresse[:port] remplace le serveur du jeu (tests, serveur de développement)
        if (const char *env = std::getenv("LARVA_ONLINE_SERVER"))
        {
            std::string v = env;
            size_t colon = v.rfind(':');
            cfg.server = colon == std::string::npos ? v : v.substr(0, colon);
            if (colon != std::string::npos)
                cfg.port = std::atoi(v.c_str() + colon + 1);
        }
        Backend wanted = cfg.backend != Backend::Auto ? cfg.backend : requestedBackend();
#ifdef LARVA_NO_STEAM
        (void)wanted;
#else
        if (wanted != Backend::Larva && cfg.steamAppId)
        {
            std::string error;
            if (SteamNet::init(cfg.steamAppId, &error))
                current = std::make_unique<SteamOnline>();
            else
                std::cout << "Steam indisponible : " << error << std::endl;
        }
#endif
        if (!current)
            current = std::make_unique<LarvaOnline>(cfg);
        std::cout << "Services en ligne : " << current->displayName() << std::endl;
        return *current;
    }

    Service &get() { return *current; }
    bool initialized() { return current != nullptr; }

    void shutdown()
    {
        if (current)
            current->shutdown();
        current.reset();
    }

    LarvaOnline *larva()
    {
        return current && current->backend() == Backend::Larva ? static_cast<LarvaOnline *>(current.get()) : nullptr;
    }
}
