#pragma once
#ifndef __TICKET_VERIFIER__
#define __TICKET_VERIFIER__
#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Pour un serveur de jeu dédié (ex. Mine Wars) : vérifie auprès de Larva Online le compte d'un joueur qui se connecte,
// sans jamais voir son mot de passe (protocole : TICKET / VERIFY_TICKET dans OnlineProtocol.h).
//
//   1. le serveur de jeu donne au joueur une audience propre à sa connexion (ex. "minewars:" + nombre aléatoire) ;
//   2. le jeu du joueur demande un ticket pour cette audience à Larva Online (LarvaOnline::requestTicket) et l'envoie ;
//   3. le serveur de jeu appelle verify(tag, ticket, audience), puis lit le résultat dans poll() (pseudo et id du compte).
//
// Une connexion TCP chiffrée (SecureChannel) vers Larva Online, ouverte et rouverte par un thread. La clé du serveur
// Larva Online est épinglée (Config::serverKey) ou mémorisée à la première connexion dans Config::knownKeysFile.
// Fonctionne sous Windows et Linux. Fichiers : ce .cpp, Engine/Core/Crypto.cpp et Engine/Core/Sha256.cpp.
class TicketVerifier
{
public:
    struct Config
    {
        std::string server = "127.0.0.1";
        int port = 27020;
        std::string serverKey;     // 64 caractères hexadécimaux ; vide : mémorisée dans knownKeysFile
        std::string knownKeysFile; // vide : rien n'est mémorisé (n'importe quelle clé est acceptée)
        std::string game = "server"; // identifiant annoncé à Larva Online (HELLO)
        double timeout = 10.0;     // une vérification sans réponse échoue au bout de ce délai
    };
    struct Result
    {
        uint64_t tag = 0;          // donné à verify (ex. numéro de connexion du joueur)
        bool ok = false;
        uint64_t user = 0;         // compte Larva Online
        std::string name, game;    // pseudo, jeu qui a demandé le ticket
        std::string error;
    };

    ~TicketVerifier() { stop(); }
    void start(const Config &config);
    void stop();
    void verify(uint64_t tag, const std::string &ticket, const std::string &audience);
    // Résultats arrivés depuis le dernier appel
    std::vector<Result> poll();
    bool connected() const { return isConnected; }
    // Dernier problème de connexion (serveur injoignable, clé refusée...), vide si tout va bien
    std::string lastProblem();

private:
    struct Request
    {
        uint64_t tag;
        std::string ticket, audience;
        double deadline;
        bool sent = false;
    };
    Config cfg;
    std::thread worker;
    std::atomic<bool> stopping{false}, isConnected{false};
    std::mutex mutex;
    std::deque<Request> requests;  // en attente ou envoyées
    std::vector<Result> results;
    std::string problem;
    void run();
    void fail(const std::string &ticket, const std::string &error); // mutex tenu
};

#endif
