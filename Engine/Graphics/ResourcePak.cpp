#include "ResourcePak.h"

// Static member initialization
std::ifstream ResourcePak::pakFile;
bool ResourcePak::initialized = false;
std::string ResourcePak::pakPath = "";
std::map<std::string, PakEntry> ResourcePak::entries;