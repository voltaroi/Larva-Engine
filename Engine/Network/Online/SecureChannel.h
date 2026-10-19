#pragma once
#ifndef __SECURE_CHANNEL__
#define __SECURE_CHANNEL__
#include <cstdint>
#include <cstring>
#include <string>
#include "Engine/Core/Crypto.h"

// Chiffrement de la connexion TCP entre un jeu et le serveur Larva Online.
//
// Poignée de main (inspirée de Noise NK), avant tout autre octet :
//   client -> serveur : "LOS1" + clé éphémère du client Ec            (36 octets)
//   serveur -> client : "LOS1" + clé éphémère du serveur Es + clé fixe du serveur S   (68 octets)
//   secret = X25519(ec, S) || X25519(ec, Es)   (côté serveur : X25519(s, Ec) || X25519(es, Ec))
//   clés   = HKDF(sel = SHA256("larva-online-1" || S || Ec || Es), secret) -> client->serveur, serveur->client
// Seul le détenteur de la clé privée du serveur peut calculer le secret : le client vérifie S (clé épinglée dans le
// jeu, ou mémorisée à la première connexion), et un faux serveur ne peut ni lire ni répondre. Les clés éphémères
// rendent chaque connexion indépendante (une clé du serveur volée plus tard ne déchiffre pas les connexions passées).
//
// Ensuite chaque trame est [taille u32][ChaCha20-Poly1305(contenu)], la taille servant de donnée authentifiée et un
// compteur par sens servant de nonce : une trame modifiée, rejouée ou réordonnée coupe la connexion.
class SecureChannel
{
public:
    static const size_t HELLO_SIZE = 36, REPLY_SIZE = 68;

    // --- Client ---
    std::string clientHello()
    {
        larva::crypto::x25519Keypair(ePriv, ePub);
        return std::string("LOS1") + std::string((const char *)ePub, 32);
    }
    // reply : les 68 octets du serveur ; serverKey reçoit S (à vérifier par l'appelant avant d'envoyer quoi que ce soit)
    bool clientFinish(const uint8_t *reply, uint8_t serverKey[32])
    {
        if (std::memcmp(reply, "LOS1", 4) != 0)
            return false;
        const uint8_t *es = reply + 4, *s = reply + 36;
        std::memcpy(serverKey, s, 32);
        uint8_t secret[64];
        if (!larva::crypto::x25519(secret, ePriv, s) || !larva::crypto::x25519(secret + 32, ePriv, es))
            return false;
        derive(s, ePub, es, secret, false);
        return true;
    }

    // --- Serveur ---
    bool serverAccept(const uint8_t sPriv[32], const uint8_t sPub[32], const uint8_t *hello, std::string &reply)
    {
        if (std::memcmp(hello, "LOS1", 4) != 0)
            return false;
        const uint8_t *ec = hello + 4;
        uint8_t esPriv[32], esPub[32];
        larva::crypto::x25519Keypair(esPriv, esPub);
        uint8_t secret[64];
        if (!larva::crypto::x25519(secret, sPriv, ec) || !larva::crypto::x25519(secret + 32, esPriv, ec))
            return false;
        derive(sPub, ec, esPub, secret, true);
        reply = std::string("LOS1") + std::string((const char *)esPub, 32) + std::string((const char *)sPub, 32);
        return true;
    }

    bool ready() const { return isReady; }

    // Trame chiffrée complète (taille comprise) pour ce contenu
    std::string seal(const std::string &body)
    {
        std::string out(4 + body.size() + larva::crypto::TAG, '\0');
        uint32_t len = (uint32_t)(body.size() + larva::crypto::TAG);
        for (int i = 0; i < 4; ++i)
            out[i] = (char)((len >> (8 * i)) & 0xFF);
        larva::crypto::seal(sendKey, sendNonce++, out.data(), 4, body.data(), body.size(), (uint8_t *)&out[4]);
        return out;
    }

    // Déchiffre le contenu d'une trame (sans les 4 octets de taille) ; false : trame falsifiée
    bool open(const std::string &cipher, std::string &body)
    {
        if (cipher.size() < larva::crypto::TAG)
            return false;
        uint8_t lenBytes[4];
        uint32_t len = (uint32_t)cipher.size();
        for (int i = 0; i < 4; ++i)
            lenBytes[i] = (uint8_t)((len >> (8 * i)) & 0xFF);
        body.resize(cipher.size() - larva::crypto::TAG);
        return larva::crypto::open(recvKey, recvNonce++, lenBytes, 4, cipher.data(), cipher.size(), (uint8_t *)&body[0]);
    }

private:
    uint8_t ePriv[32], ePub[32];
    uint8_t sendKey[32], recvKey[32];
    uint64_t sendNonce = 0, recvNonce = 0;
    bool isReady = false;

    void derive(const uint8_t s[32], const uint8_t ec[32], const uint8_t es[32], const uint8_t secret[64], bool server)
    {
        std::string transcript = "larva-online-1";
        transcript.append((const char *)s, 32);
        transcript.append((const char *)ec, 32);
        transcript.append((const char *)es, 32);
        uint8_t salt[32], keys[64];
        larva::crypto::sha256(transcript.data(), transcript.size(), salt);
        larva::crypto::hkdf(salt, 32, secret, 64, "c2s|s2c", 7, keys, 64);
        std::memcpy(server ? recvKey : sendKey, keys, 32);
        std::memcpy(server ? sendKey : recvKey, keys + 32, 32);
        isReady = true;
    }
};

#endif
