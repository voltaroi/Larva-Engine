#pragma once
#ifndef __ONLINE_PROTOCOL__
#define __ONLINE_PROTOCOL__
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "Engine/Core/Sha256.h"

// Protocole Larva Online, partagé par le serveur (projects/online) et le client du moteur (LarvaOnline).
//
// Une seule connexion TCP sortante par jeu (ou launcher) vers le serveur : aucun port à ouvrir chez les joueurs.
// Elle est chiffrée dès le premier octet (SecureChannel.h). Chaque trame déchiffrée commence par son type :
//   'C' commande texte, champs séparés par des tabulations : "LOGIN\tpseudo\tsecret"
//   'D' données de jeu relayées : [joueur u64 petit-boutiste][morceaux]. Du client vers le serveur, le joueur est le
//       destinataire ; du serveur vers le client, c'est l'expéditeur. Le serveur ne relaie qu'entre membres d'un même salon.
//
// Connexion directe entre joueurs (UDP, traversée des box) : voir LarvaOnline.cpp. Le serveur écoute aussi en UDP sur
// le même port pour découvrir l'adresse publique de chaque joueur ("LOU1" + jeton), puis donne à chaque paire de membres
// d'un salon les adresses de l'autre et une clé commune (PEER_INFO). Les messages sont découpés en morceaux numérotés
// qui passent indifféremment par le direct ou par le relais : si le direct tombe, ce qui n'a pas été confirmé repart
// par le relais, sans perte ni désordre.
//
// Commandes du client                         Réponses / événements du serveur
//   HELLO version jeu                           WELCOME version
//   REGISTER pseudo secret                      AUTH id pseudo jeton        (connecté)
//   LOGIN pseudo secret                         ERR contexte message
//   RESUME pseudo jeton                         FRIEND id pseudo état jeu salon joignable   (ajout ou mise à jour)
//   LOGOUT                                      FRIEND_GONE id
//   FRIEND_ADD pseudo                           LOBBY_ENTERED salon propriétaire max [id pseudo]... (tous les membres)
//   FRIEND_ACCEPT id / FRIEND_REMOVE id         LOBBY_FAILED message
//   LOBBY_CREATE max                            MEMBER id pseudo            (nouveau membre)
//   LOBBY_JOIN salon                            MEMBER_LEFT id
//   LOBBY_LEAVE                                 LOBBY_CLOSED                (le propriétaire est parti)
//   INVITE id                                   INVITED id pseudo salon jeu
//   PING                                        PEER_FAILED id              (données vers un joueur hors du salon)
//   UDP_LOCAL ip port                           PONG
//     (adresse sur le réseau local : joueurs    UDP_TOKEN jeton             (à envoyer en UDP au serveur)
//      derrière la même box)                    UDP_SEEN ip port            (adresse publique vue par le serveur)
//                                               PEER_INFO id paire clé ip port ip_locale port_local  (avant MEMBER / LOBBY_ENTERED)
//                                               PEER_ADDR id ip port ip_locale port_local            (adresses mises à jour)
// Le secret n'est jamais le mot de passe : c'est secretOf(pseudo, mot de passe), calculé par le client.
//
// Serveurs de jeu dédiés (ex. Mine Wars) : le joueur prouve son compte Larva Online sans donner son mot de passe.
//   joueur connecté :   TICKET audience             TICKET audience ticket   / TICKET_FAILED audience raison
//   serveur de jeu :    VERIFY_TICKET ticket audience   TICKET_OK ticket id pseudo jeu / TICKET_BAD ticket raison
// Le ticket (aléatoire, 128 bits) vaut 60 s et une seule vérification. L'audience est choisie par le serveur de jeu
// (ex. "minewars:" + nombre aléatoire propre à la connexion du joueur) : un ticket intercepté ne sert sur aucune autre
// connexion. VERIFY_TICKET ne demande pas de compte : le serveur de jeu ouvre sa propre connexion (HELLO puis vérifs).
namespace OnlineProto
{
    const int VERSION = 2;
    const int DEFAULT_PORT = 27020;
    const uint32_t MAX_FRAME = 1u << 20; // 1 Mo (contenu déchiffré)
    const int MAX_LOBBY = 64;

    // États d'un ami (champ "état" de FRIEND)
    const int OFFLINE = 0, ONLINE = 1, REQUEST_IN = 2, REQUEST_OUT = 3;

    inline std::string lower(std::string s)
    {
        for (char &c : s)
            c = (char)std::tolower((unsigned char)c);
        return s;
    }

    // Pseudo : 3 à 16 caractères, lettres, chiffres, _ ou -
    inline bool validName(const std::string &name)
    {
        if (name.size() < 3 || name.size() > 16)
            return false;
        for (char c : name)
            if (!std::isalnum((unsigned char)c) && c != '_' && c != '-')
                return false;
        return true;
    }

    // Identifiant de jeu ("rempart", "launcher"...) : 1 à 32 caractères, lettres, chiffres, _ ou -
    inline bool validGame(const std::string &game)
    {
        if (game.empty() || game.size() > 32)
            return false;
        for (char c : game)
            if (!std::isalnum((unsigned char)c) && c != '_' && c != '-')
                return false;
        return true;
    }

    inline void putU32(std::string &out, uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            out += (char)((v >> (8 * i)) & 0xFF);
    }
    inline void putU64(std::string &out, uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
            out += (char)((v >> (8 * i)) & 0xFF);
    }
    inline uint32_t getU32(const char *p)
    {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= (uint32_t)(uint8_t)p[i] << (8 * i);
        return v;
    }
    inline uint64_t getU64(const char *p)
    {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v |= (uint64_t)(uint8_t)p[i] << (8 * i);
        return v;
    }

    // Contenu d'une trame (avant chiffrement)
    inline std::string commandBody(const std::vector<std::string> &fields)
    {
        std::string body = "C";
        for (size_t i = 0; i < fields.size(); ++i)
        {
            if (i)
                body += '\t';
            body += fields[i];
        }
        return body;
    }

    inline std::string dataBody(uint64_t peer, const std::string &chunks)
    {
        std::string out = "D";
        putU64(out, peer);
        return out + chunks;
    }

    // Morceau de message (direct ou relais) : [n° u32][drapeaux u8][taille u32][octets]
    const uint8_t CHUNK_LAST = 1; // dernier morceau du message
    const size_t CHUNK_HEADER = 9;
    inline void putChunk(std::string &out, uint32_t seq, uint8_t flags, const std::string &data)
    {
        putU32(out, seq);
        out += (char)flags;
        putU32(out, (uint32_t)data.size());
        out += data;
    }

    // UDP vers le serveur : "LOU1" + jeton (UDP_TOKEN)
    const char *const UDP_HELLO = "LOU1";
    // UDP entre joueurs : [0x4C][paire u32][nonce u64] + ChaCha20-Poly1305([type u8][accusé u32][reçus u64][morceaux]),
    // l'en-tête servant de donnée authentifiée
    const uint8_t P2P_MAGIC = 0x4C;
    const size_t P2P_HEADER = 13, P2P_MAX = 1200, P2P_CHUNK = 1100;
    const uint8_t P2P_PROBE = 1, P2P_PROBE_ACK = 2, P2P_DATA = 3;

    // Lit la trame suivante du flux reçu à partir de offset (avancé d'autant).
    // Renvoie 1 : trame lue dans frame, 0 : trame incomplète (attendre la suite), -1 : flux invalide (couper la connexion)
    inline int nextFrame(const std::string &buffer, size_t &offset, std::string &frame)
    {
        if (buffer.size() - offset < 4)
            return 0;
        uint32_t size = getU32(buffer.data() + offset);
        if (size == 0 || size > MAX_FRAME + 16)
            return -1;
        if (buffer.size() - offset - 4 < size)
            return 0;
        frame.assign(buffer, offset + 4, size);
        offset += 4 + size;
        return 1;
    }

    // Secret envoyé au serveur à la place du mot de passe (le serveur le ré-empreinte avec un sel)
    inline std::string secretOf(const std::string &name, const std::string &password)
    {
        return larva::sha256Text("larva-online|" + lower(name) + "|" + password);
    }

    inline std::vector<std::string> split(const std::string &text)
    {
        std::vector<std::string> out(1);
        for (char c : text)
        {
            if (c == '\t')
                out.emplace_back();
            else
                out.back() += c;
        }
        return out;
    }

    // Texte libre (messages d'erreur) : pas de tabulation ni de retour à la ligne
    inline std::string clean(std::string s)
    {
        for (char &c : s)
            if (c == '\t' || c == '\n' || c == '\r')
                c = ' ';
        return s;
    }
}

#endif
