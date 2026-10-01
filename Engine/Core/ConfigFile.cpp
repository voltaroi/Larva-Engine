#include "ConfigFile.h"
#include <fstream>
#include <sstream>

bool ConfigFile::load(const std::string &path)
{
    std::ifstream f(path);
    if (!f)
        return false;
    std::string line;
    while (std::getline(f, line))
    {
        std::istringstream iss(line);
        std::string key, value;
        if (!(iss >> key) || key[0] == '#')
            continue;
        std::getline(iss >> std::ws, value);
        while (!value.empty() && (value.back() == '\r' || value.back() == ' '))
            value.pop_back();
        values[key] = value;
    }
    return true;
}

bool ConfigFile::save(const std::string &path) const
{
    std::ofstream f(path);
    if (!f)
        return false;
    for (const auto &kv : values)
        f << kv.first << " " << kv.second << "\n";
    return true;
}

std::string ConfigFile::getString(const std::string &key, const std::string &fallback) const
{
    auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

float ConfigFile::getFloat(const std::string &key, float fallback) const
{
    auto it = values.find(key);
    if (it == values.end())
        return fallback;
    try
    {
        return std::stof(it->second);
    }
    catch (...)
    {
        return fallback;
    }
}

int ConfigFile::getInt(const std::string &key, int fallback) const
{
    auto it = values.find(key);
    if (it == values.end())
        return fallback;
    try
    {
        return std::stoi(it->second);
    }
    catch (...)
    {
        return fallback;
    }
}

bool ConfigFile::getBool(const std::string &key, bool fallback) const
{
    auto it = values.find(key);
    if (it == values.end())
        return fallback;
    const std::string &v = it->second;
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

void ConfigFile::set(const std::string &key, float value)
{
    std::ostringstream oss;
    oss << value;
    values[key] = oss.str();
}
