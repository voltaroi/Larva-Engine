#include "Level.h"
#include <cstdio>
#include <fstream>
#include <sstream>

static const int LEVEL_FORMAT_VERSION = 1;

static std::string formatFloat(float v)
{
    if (v == 0.0f)
        v = 0.0f; // évite "-0"
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", v);
    std::string s = buf;
    // Retire les zéros inutiles : "1.5000" -> "1.5", "2.0000" -> "2"
    size_t dot = s.find('.');
    if (dot != std::string::npos)
    {
        size_t last = s.find_last_not_of('0');
        if (last == dot)
            last--;
        s.erase(last + 1);
    }
    if (s == "-0")
        s = "0";
    return s;
}

static std::string trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static bool readFloats(const std::string &value, float *out, int count)
{
    std::istringstream iss(value);
    for (int i = 0; i < count; i++)
    {
        if (!(iss >> out[i]))
            return false;
    }
    return true;
}

static std::string vecToString(const float *v, int count)
{
    std::string s;
    for (int i = 0; i < count; i++)
    {
        if (i)
            s += ' ';
        s += formatFloat(v[i]);
    }
    return s;
}

std::string LevelEntity::property(const std::string &key, const std::string &fallback) const
{
    auto it = properties.find(key);
    return it == properties.end() ? fallback : it->second;
}

const char *LevelEntity::typeName(Type type)
{
    switch (type)
    {
    case Type::PlayerStart:
        return "player_start";
    case Type::Marker:
        return "marker";
    default:
        return "mesh";
    }
}

bool LevelEntity::typeFromName(const std::string &name, Type &out)
{
    if (name == "mesh")
        out = Type::Mesh;
    else if (name == "player_start")
        out = Type::PlayerStart;
    else if (name == "marker")
        out = Type::Marker;
    else
        return false;
    return true;
}

void Level::clear()
{
    *this = Level();
}

bool Level::loadFromFile(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
    {
        error = "cannot open " + path;
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return loadFromString(ss.str());
}

bool Level::loadFromString(const std::string &text)
{
    Level result;
    std::istringstream in(text);
    std::string line;
    int lineNumber = 0;
    LevelEntity *current = nullptr;
    bool headerSeen = false;

    auto fail = [&](const std::string &message)
    {
        error = "line " + std::to_string(lineNumber) + ": " + message;
        return false;
    };

    while (std::getline(in, line))
    {
        lineNumber++;
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        size_t sp = line.find_first_of(" \t");
        std::string key = line.substr(0, sp);
        std::string value = sp == std::string::npos ? "" : trim(line.substr(sp));

        if (!headerSeen)
        {
            if (key != "larva_level")
                return fail("missing larva_level header");
            headerSeen = true;
            continue;
        }

        if (current == nullptr)
        {
            if (key == "name")
                result.name = value;
            else if (key == "sky")
            {
                if (!readFloats(value, result.skyColor, 3))
                    return fail("sky expects 3 numbers");
            }
            else if (key == "light_position")
            {
                if (!readFloats(value, result.lightPosition, 3))
                    return fail("light_position expects 3 numbers");
            }
            else if (key == "light_target")
            {
                if (!readFloats(value, result.lightTarget, 3))
                    return fail("light_target expects 3 numbers");
            }
            else if (key == "shadow_area")
            {
                if (!readFloats(value, &result.shadowArea, 1))
                    return fail("shadow_area expects a number");
            }
            else if (key == "entity")
            {
                result.entities.emplace_back();
                current = &result.entities.back();
                current->name = value;
            }
            // Clés inconnues ignorées : compatibilité avec de futures versions
            continue;
        }

        // Dans un bloc entity ... end
        if (key == "end")
            current = nullptr;
        else if (key == "type")
        {
            if (!LevelEntity::typeFromName(value, current->type))
                return fail("unknown entity type " + value);
        }
        else if (key == "model")
            current->model = value;
        else if (key == "position")
        {
            if (!readFloats(value, current->position, 3))
                return fail("position expects 3 numbers");
        }
        else if (key == "rotation")
        {
            if (!readFloats(value, current->rotation, 3))
                return fail("rotation expects 3 numbers");
        }
        else if (key == "scale")
        {
            if (!readFloats(value, current->scale, 3))
                return fail("scale expects 3 numbers");
        }
        else if (key == "color")
        {
            if (!readFloats(value, current->color, 4))
                return fail("color expects 4 numbers");
            current->useColor = true;
        }
        else if (key == "collision")
            current->collision = (value == "1" || value == "true");
        else if (key == "visible")
            current->visible = !(value == "0" || value == "false");
        else if (key == "tag")
            current->tag = value;
        else if (key == "prop")
        {
            size_t s = value.find_first_of(" \t");
            std::string k = value.substr(0, s);
            std::string v = s == std::string::npos ? "" : trim(value.substr(s));
            if (!k.empty())
                current->properties[k] = v;
        }
    }

    if (!headerSeen)
        return fail("empty level file");
    if (current != nullptr)
        return fail("entity " + current->name + " is missing its end line");

    *this = std::move(result);
    return true;
}

std::string Level::saveToString() const
{
    std::ostringstream out;
    out << "larva_level " << LEVEL_FORMAT_VERSION << "\n";
    out << "name " << name << "\n";
    out << "sky " << vecToString(skyColor, 3) << "\n";
    out << "light_position " << vecToString(lightPosition, 3) << "\n";
    out << "light_target " << vecToString(lightTarget, 3) << "\n";
    out << "shadow_area " << formatFloat(shadowArea) << "\n";

    for (const LevelEntity &e : entities)
    {
        out << "\nentity " << e.name << "\n";
        out << "type " << LevelEntity::typeName(e.type) << "\n";
        if (!e.model.empty())
            out << "model " << e.model << "\n";
        out << "position " << vecToString(e.position, 3) << "\n";
        out << "rotation " << vecToString(e.rotation, 3) << "\n";
        out << "scale " << vecToString(e.scale, 3) << "\n";
        if (e.useColor)
            out << "color " << vecToString(e.color, 4) << "\n";
        if (e.collision)
            out << "collision 1\n";
        if (!e.visible)
            out << "visible 0\n";
        if (!e.tag.empty())
            out << "tag " << e.tag << "\n";
        for (const auto &p : e.properties)
            out << "prop " << p.first << " " << p.second << "\n";
        out << "end\n";
    }
    return out.str();
}

bool Level::saveToFile(const std::string &path) const
{
    // Écriture dans un fichier temporaire puis remplacement : pas de niveau tronqué si l'écriture échoue
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f.is_open())
            return false;
        f << saveToString();
        if (!f.good())
            return false;
    }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

LevelEntity *Level::find(const std::string &entityName)
{
    for (auto &e : entities)
        if (e.name == entityName)
            return &e;
    return nullptr;
}

const LevelEntity *Level::find(const std::string &entityName) const
{
    for (const auto &e : entities)
        if (e.name == entityName)
            return &e;
    return nullptr;
}

std::vector<const LevelEntity *> Level::findByTag(const std::string &tagName) const
{
    std::vector<const LevelEntity *> result;
    for (const auto &e : entities)
        if (e.tag == tagName)
            result.push_back(&e);
    return result;
}

const LevelEntity *Level::playerStart() const
{
    for (const auto &e : entities)
        if (e.type == LevelEntity::Type::PlayerStart)
            return &e;
    return nullptr;
}

std::string Level::uniqueName(const std::string &base) const
{
    if (!find(base))
        return base;
    // Retire un suffixe "_N" existant pour repartir de la racine
    std::string root = base;
    size_t us = root.find_last_of('_');
    if (us != std::string::npos && us + 1 < root.size() && root.find_first_not_of("0123456789", us + 1) == std::string::npos)
        root = root.substr(0, us);
    for (int i = 2;; i++)
    {
        std::string candidate = root + "_" + std::to_string(i);
        if (!find(candidate))
            return candidate;
    }
}
