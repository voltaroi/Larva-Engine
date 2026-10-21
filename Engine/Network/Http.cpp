#include "Http.h"
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

static std::wstring widen(const std::string &s)
{
    if (s.empty())
        return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static std::string winError(const char *what, DWORD code)
{
    switch (code)
    {
    case ERROR_WINHTTP_TIMEOUT:
        return std::string(what) + " : delai depasse";
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
        return std::string(what) + " : serveur introuvable";
    case ERROR_WINHTTP_CANNOT_CONNECT:
        return std::string(what) + " : connexion refusee";
    case ERROR_WINHTTP_CONNECTION_ERROR:
        return std::string(what) + " : connexion perdue";
    case ERROR_WINHTTP_SECURE_FAILURE:
        return std::string(what) + " : certificat HTTPS refuse";
    default:
        return std::string(what) + " : erreur reseau " + std::to_string((unsigned long)code);
    }
}

std::string Http::encodePath(const std::string &path)
{
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : path)
    {
        bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                     c == '.' || c == '~' || c == '/';
        if (plain)
            out += (char)c;
        else
        {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

bool Http::setBase(const std::string &url, std::string &error)
{
    close();
    baseSet = false;
    std::string rest;
    if (url.compare(0, 7, "http://") == 0)
    {
        secure = false;
        port = 80;
        rest = url.substr(7);
    }
    else if (url.compare(0, 8, "https://") == 0)
    {
        secure = true;
        port = 443;
        rest = url.substr(8);
    }
    else
    {
        error = "Adresse du serveur invalide (http:// ou https:// attendu)";
        return false;
    }
    size_t slash = rest.find('/');
    std::string hostPort = rest.substr(0, slash);
    std::string pre = slash == std::string::npos ? "" : rest.substr(slash);
    while (!pre.empty() && pre.back() == '/')
        pre.pop_back();
    size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos && hostPort.find(']') == std::string::npos)
    {
        int p = std::atoi(hostPort.c_str() + colon + 1);
        if (p <= 0 || p > 65535)
        {
            error = "Port invalide dans l'adresse du serveur";
            return false;
        }
        port = (INTERNET_PORT)p;
        hostPort = hostPort.substr(0, colon);
    }
    if (hostPort.empty())
    {
        error = "Adresse du serveur vide";
        return false;
    }
    host = widen(hostPort);
    prefix = widen(pre);
    baseSet = true;
    return true;
}

bool Http::open(std::string &error)
{
    if (connection)
        return true;
    if (!baseSet)
    {
        error = "Serveur non configure";
        return false;
    }
    session = WinHttpOpen(L"LarvaEngine/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
    {
        error = winError("Initialisation reseau", GetLastError());
        return false;
    }
    WinHttpSetTimeouts(session, connectTimeout, connectTimeout, connectTimeout, receiveTimeout);
    connection = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connection)
    {
        error = winError("Connexion", GetLastError());
        close();
        return false;
    }
    return true;
}

void Http::setTimeouts(int connectMs, int receiveMs)
{
    connectTimeout = connectMs;
    receiveTimeout = receiveMs;
    if (session)
        WinHttpSetTimeouts(session, connectTimeout, connectTimeout, connectTimeout, receiveTimeout);
}

void Http::close()
{
    if (connection)
        WinHttpCloseHandle(connection);
    if (session)
        WinHttpCloseHandle(session);
    connection = nullptr;
    session = nullptr;
}

// Envoie la requete (GET, ou POST avec data) et attend l'en-tete de la reponse ; une connexion coupee entre deux requetes
// (serveur qui ferme le keep-alive) est rouverte une fois.
HINTERNET Http::request(const std::string &path, const std::wstring &extraHeaders, std::string &error, const wchar_t *method,
                        const std::string *data)
{
    for (int attempt = 0; attempt < 2; attempt++)
    {
        if (!open(error))
            return nullptr;
        std::wstring full = prefix + widen(path);
        HINTERNET req = WinHttpOpenRequest(connection, method, full.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           secure ? WINHTTP_FLAG_SECURE : 0);
        DWORD code = 0;
        if (req)
        {
            BOOL sent = WinHttpSendRequest(req, extraHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extraHeaders.c_str(),
                                           extraHeaders.empty() ? 0 : (DWORD)-1L,
                                           data ? (LPVOID)data->data() : WINHTTP_NO_REQUEST_DATA, data ? (DWORD)data->size() : 0,
                                           data ? (DWORD)data->size() : 0, 0) &&
                         WinHttpReceiveResponse(req, nullptr);
            if (sent)
                return req;
            code = GetLastError();
            WinHttpCloseHandle(req);
        }
        else
            code = GetLastError();
        close(); // connexion morte : on repart de zero
        if (attempt == 1 || code == ERROR_WINHTTP_NAME_NOT_RESOLVED || code == ERROR_WINHTTP_CANNOT_CONNECT || code == ERROR_WINHTTP_TIMEOUT)
        {
            error = winError("Requete", code);
            return nullptr;
        }
    }
    return nullptr;
}

static int statusOf(HINTERNET req)
{
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                        WINHTTP_NO_HEADER_INDEX);
    return (int)status;
}

Http::Result Http::get(const std::string &path, std::string &body, const std::atomic<bool> *cancel)
{
    Result r;
    body.clear();
    HINTERNET req = request(encodePath(path), L"", r.error);
    if (!req)
        return r;
    return readBody(req, body, cancel);
}

Http::Result Http::post(const std::string &path, const std::string &data, const std::string &contentType, std::string &body,
                        const std::atomic<bool> *cancel)
{
    Result r;
    body.clear();
    HINTERNET req = request(encodePath(path), L"Content-Type: " + widen(contentType) + L"\r\n", r.error, L"POST", &data);
    if (!req)
        return r;
    return readBody(req, body, cancel);
}

// Lit tout le corps de la reponse puis ferme la requete
Http::Result Http::readBody(HINTERNET req, std::string &body, const std::atomic<bool> *cancel)
{
    Result r;
    r.status = statusOf(req);
    std::vector<char> chunk(64 * 1024);
    while (true)
    {
        if (cancel && *cancel)
        {
            r.error = "Annule";
            break;
        }
        DWORD got = 0;
        if (!WinHttpReadData(req, chunk.data(), (DWORD)chunk.size(), &got))
        {
            r.error = winError("Lecture", GetLastError());
            break;
        }
        if (got == 0)
            break;
        body.append(chunk.data(), got);
        if (body.size() > 64u * 1024 * 1024)
        {
            r.error = "Reponse trop grande";
            break;
        }
    }
    WinHttpCloseHandle(req);
    return r;
}

Http::Result Http::download(const std::string &path, const std::string &destFile, uint64_t expectedSize,
                            const std::function<void(uint64_t)> &onStart, const std::function<void(uint64_t)> &onBytes,
                            const std::atomic<bool> *cancel)
{
    Result r;
    fs::path dest = fs::u8path(destFile);
    std::error_code ec;
    uint64_t have = fs::exists(dest, ec) ? (uint64_t)fs::file_size(dest, ec) : 0;
    if (have > expectedSize)
    {
        fs::remove(dest, ec);
        have = 0;
    }

    std::wstring headers;
    if (have > 0)
        headers = L"Range: bytes=" + std::to_wstring(have) + L"-\r\n";
    HINTERNET req = request(encodePath(path), headers, r.error);
    if (!req)
        return r;
    r.status = statusOf(req);

    if (r.status == 416 && have > 0)
    {
        // Le serveur n'a plus ce fichier tel quel : on repart de zero au prochain essai
        WinHttpCloseHandle(req);
        fs::remove(dest, ec);
        r.error = "Reprise impossible, nouvel essai";
        return r;
    }
    if (r.status != 200 && r.status != 206)
    {
        WinHttpCloseHandle(req);
        if (r.status == 404)
            r.error = "Fichier introuvable sur le serveur";
        else
            r.error = "Reponse du serveur : " + std::to_string(r.status);
        return r;
    }

    bool append = r.status == 206 && have > 0;
    if (!append)
        have = 0; // le serveur renvoie tout le fichier
    std::ofstream out(dest, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
    if (!out)
    {
        WinHttpCloseHandle(req);
        r.error = "Impossible d'ecrire " + destFile;
        return r;
    }

    if (onStart)
        onStart(have);
    std::vector<char> chunk(256 * 1024);
    while (true)
    {
        if (cancel && *cancel)
        {
            r.error = "Annule";
            break;
        }
        DWORD got = 0;
        if (!WinHttpReadData(req, chunk.data(), (DWORD)chunk.size(), &got))
        {
            r.error = winError("Telechargement", GetLastError());
            break;
        }
        if (got == 0)
            break;
        out.write(chunk.data(), got);
        if (!out)
        {
            r.error = "Ecriture impossible (disque plein ?)";
            break;
        }
        if (onBytes)
            onBytes(got);
    }
    out.close();
    WinHttpCloseHandle(req);
    return r;
}
