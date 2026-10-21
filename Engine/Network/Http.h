#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <windows.h>
#include <winhttp.h>

// Requetes HTTP / HTTPS avec WinHTTP (fourni par Windows : pas de bibliotheque a embarquer, HTTPS et proxy du
// systeme geres). Un objet Http n'est pas partage entre threads : un thread = un objet, qui garde sa connexion
// ouverte entre deux requetes.
class Http
{
public:
    struct Result
    {
        int status = 0;     // code HTTP (0 : pas de reponse)
        std::string error;  // vide si la requete a abouti (meme avec un code 404)
        bool ok() const { return error.empty() && status >= 200 && status < 300; }
    };

    Http() = default;
    ~Http() { close(); }
    Http(const Http &) = delete;
    Http &operator=(const Http &) = delete;

    // "http://hote:port" ou "https://hote" (un prefixe de chemin est conserve : https://site.fr/launcher)
    bool setBase(const std::string &url, std::string &error);

    // Delais en millisecondes : connexion / envoi, puis attente de chaque morceau de la reponse
    void setTimeouts(int connectMs, int receiveMs);

    // Petite reponse en memoire (liste des jeux, manifeste)
    Result get(const std::string &path, std::string &body, const std::atomic<bool> *cancel = nullptr);

    // POST d'un corps (JSON, WAV...) et reponse en memoire
    Result post(const std::string &path, const std::string &data, const std::string &contentType, std::string &body,
                const std::atomic<bool> *cancel = nullptr);

    // Telecharge vers un fichier. Si le fichier existe deja avec moins de expectedSize octets, le telechargement
    // reprend a cet endroit (en-tete Range). onStart recoit une fois l'octet ou commence le corps de la reponse
    // (0 si le serveur renvoie tout), puis onBytes le nombre d'octets ecrits a chaque paquet.
    Result download(const std::string &path, const std::string &destFile, uint64_t expectedSize,
                    const std::function<void(uint64_t)> &onStart, const std::function<void(uint64_t)> &onBytes,
                    const std::atomic<bool> *cancel = nullptr);

    static std::string encodePath(const std::string &path);

private:
    bool open(std::string &error);
    void close();
    HINTERNET request(const std::string &path, const std::wstring &extraHeaders, std::string &error,
                      const wchar_t *method = L"GET", const std::string *data = nullptr);
    Result readBody(HINTERNET req, std::string &body, const std::atomic<bool> *cancel);

    std::wstring host;
    std::wstring prefix;
    INTERNET_PORT port = 80;
    bool secure = false;
    bool baseSet = false;
    int connectTimeout = 8000;
    int receiveTimeout = 30000;
    HINTERNET session = nullptr;
    HINTERNET connection = nullptr;
};
