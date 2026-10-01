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
}

#endif
