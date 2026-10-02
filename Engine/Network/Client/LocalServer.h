#pragma once
#ifndef __LOCAL_SERVER__
#define __LOCAL_SERVER__
#include <string>
#include <vector>

// Lance l'exécutable serveur du jeu sur ce PC pour héberger une partie (bouton "Héberger"),
// et l'arrête à la fermeture du client. Windows uniquement.
namespace LocalServer
{
    // Cherche exeName à côté du client puis dans ..\Server\ et le lance avec les arguments donnés
    bool launch(const std::string &exeName, const std::vector<std::string> &args, std::string &error);
    void stop();
    bool isRunning();

    // Adresse IPv4 locale de ce PC (celle à donner aux autres joueurs du réseau)
    std::string localIp();

    // Toutes les adresses IPv4 de ce PC, avec le réseau reconnu : "Hamachi" (25.x), "Radmin VPN" (26.x),
    // "Tailscale" (100.64 à 100.127), "ZeroTier" ou "reseau local"
    struct Address
    {
        std::string ip, network;
        bool vpn;
    };
    std::vector<Address> localAddresses();
    // Adresses à donner aux autres joueurs, VPN en premier : "Hamachi 25.1.2.3  |  reseau local 192.168.1.20"
    std::string shareText();

    // Autorise les connexions entrantes vers exeName dans le pare-feu Windows, sur tous les réseaux (les VPN
    // comme Hamachi sont souvent classés "publics"). Si la règle n'existe pas encore, Windows demande une fois
    // l'autorisation administrateur. Deux règles : le programme, et le port TCP (certains PC ignorent la règle
    // par programme). Renvoie true si les règles existent ou viennent d'être créées.
    bool ensureFirewallRule(const std::string &ruleName, const std::string &exeName, int port, std::string &error);
}

#endif
