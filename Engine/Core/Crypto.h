#pragma once
#ifndef __LARVA_CRYPTO__
#define __LARVA_CRYPTO__
#include <cstddef>
#include <cstdint>
#include <string>

// Cryptographie sans dépendance pour Larva Online (connexion au serveur et liaisons directes entre joueurs) :
//   X25519 (échange de clés, RFC 7748), ChaCha20-Poly1305 (chiffrement authentifié, RFC 8439),
//   HMAC-SHA256 et HKDF (dérivation de clés, RFC 2104 / 5869), aléa du système.
// Vérifié avec les vecteurs de test des RFC : projects/online/test/crypto_test.cpp.
namespace larva
{
    namespace crypto
    {
        const size_t KEY = 32, TAG = 16;

        // Aléa cryptographique du système (rand_s / RtlGenRandom sous Windows, /dev/urandom ailleurs)
        void randomBytes(void *out, size_t len);

        // Clé privée X25519 aléatoire, et la clé publique correspondante
        void x25519Keypair(uint8_t priv[32], uint8_t pub[32]);
        void x25519Public(uint8_t pub[32], const uint8_t priv[32]);
        // Secret partagé ; false si le point reçu est d'ordre faible (secret nul : connexion à refuser)
        bool x25519(uint8_t out[32], const uint8_t priv[32], const uint8_t peerPub[32]);

        // Chiffrement authentifié. nonce : compteur, jamais réutilisé avec la même clé.
        // seal écrit len + TAG octets dans out ; open lit len octets (TAG compris) et écrit len - TAG octets,
        // false si le message a été modifié ou ne vient pas du détenteur de la clé
        void seal(const uint8_t key[32], uint64_t nonce, const void *aad, size_t aadLen, const void *in, size_t len, uint8_t *out);
        bool open(const uint8_t key[32], uint64_t nonce, const void *aad, size_t aadLen, const void *in, size_t len, uint8_t *out);
        // Variante RFC 8439 complète (nonce de 12 octets), pour les vecteurs de test
        void seal12(const uint8_t key[32], const uint8_t nonce[12], const void *aad, size_t aadLen, const void *in, size_t len, uint8_t *out);
        bool open12(const uint8_t key[32], const uint8_t nonce[12], const void *aad, size_t aadLen, const void *in, size_t len, uint8_t *out);
        void poly1305(uint8_t tag[16], const void *msg, size_t len, const uint8_t key[32]);

        void sha256(const void *data, size_t len, uint8_t out[32]);
        void hmacSha256(const void *key, size_t keyLen, const void *data, size_t len, uint8_t out[32]);
        void hkdf(const void *salt, size_t saltLen, const void *ikm, size_t ikmLen, const void *info, size_t infoLen, uint8_t *out, size_t outLen);

        // Comparaison en temps constant
        bool equal(const void *a, const void *b, size_t len);
        std::string toHex(const void *data, size_t len);
        // false si le texte n'est pas exactement len octets en hexadécimal
        bool fromHex(const std::string &hex, void *out, size_t len);
    }
}

#endif
