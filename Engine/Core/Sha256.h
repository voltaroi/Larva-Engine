#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

// SHA-256 sans dépendance : mots de passe et jetons des comptes (Larva Online), empreintes de fichiers.
namespace larva
{
    class Sha256
    {
    public:
        Sha256();
        void update(const void *data, size_t len);
        std::string hex(); // termine le calcul : ne plus appeler update ensuite
        void digest(uint8_t out[32]); // idem, empreinte brute (32 octets)

    private:
        void block(const uint8_t *p);
        uint32_t h[8];
        uint8_t buf[64];
        size_t bufLen = 0;
        uint64_t total = 0;
    };

    // Empreinte hexadecimale d'un fichier ; chaine vide si le fichier ne peut pas etre lu
    std::string sha256File(const std::string &path);
    std::string sha256Text(const std::string &text);
}
