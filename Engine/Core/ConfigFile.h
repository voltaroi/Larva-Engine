#pragma once
#ifndef __CONFIG_FILE__
#define __CONFIG_FILE__
#include <map>
#include <string>

// Fichier de réglages texte, une entrée "clé valeur" par ligne (options du joueur, préférences...).
// Les lignes vides et celles qui commencent par '#' sont ignorées.
class ConfigFile
{
public:
    bool load(const std::string &path);
    bool save(const std::string &path) const;

    bool has(const std::string &key) const { return values.count(key) != 0; }
    std::string getString(const std::string &key, const std::string &fallback = "") const;
    float getFloat(const std::string &key, float fallback = 0.0f) const;
    int getInt(const std::string &key, int fallback = 0) const;
    bool getBool(const std::string &key, bool fallback = false) const;

    void set(const std::string &key, const std::string &value) { values[key] = value; }
    void set(const std::string &key, float value);
    void set(const std::string &key, int value) { values[key] = std::to_string(value); }
    void set(const std::string &key, bool value) { values[key] = value ? "1" : "0"; }
    void clear() { values.clear(); }

private:
    std::map<std::string, std::string> values;
};

#endif
