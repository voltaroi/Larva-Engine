#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdint>

struct PakEntry {
    std::string path;
    uint32_t offset;
    uint32_t size;
};

class ResourcePak {
private:
    static std::ifstream pakFile;
    static bool initialized;
    static std::string pakPath;
    static std::map<std::string, PakEntry> entries;
    static const uint32_t PAK_SIGNATURE = 0x4B414C50; // "LAKP" in little-endian (PLAK)

public:
    /**
     * Initialize the resource PAK system
     * @param path Path to the .pak file
     * @return true if initialization successful
     */
    static bool Initialize(const std::string& path) {
        Close();

        pakPath = path;
        pakFile.open(path, std::ios::binary);

        if (!pakFile.is_open()) {
            std::cerr << "[ResourcePak] ERROR: Failed to open PAK file: " << path << std::endl;
            initialized = false;
            return false;
        }

        std::cout << "[ResourcePak] Successfully opened PAK file: " << path << std::endl;
        initialized = true;
        
        // Read PAK header and index
        if (!ReadPakHeader()) {
            std::cerr << "[ResourcePak] ERROR: Invalid PAK file format" << std::endl;
            Close();
            return false;
        }
        
        return true;
    }

    /**
     * Close the PAK file
     */
    static void Close() {
        if (pakFile.is_open()) {
            pakFile.close();
            entries.clear();
            initialized = false;
            std::cout << "[ResourcePak] PAK file closed" << std::endl;
        }
    }

    /**
     * Check if PAK system is initialized
     */
    static bool IsInitialized() {
        return initialized && pakFile.is_open();
    }

    /**
     * Load raw file data from PAK
     * @param resourcePath Path within the PAK (e.g., "Assets/Images/file.png")
     * @param outData Output vector to store file contents
     * @return true if file loaded successfully
     */
    static bool LoadFile(const std::string& resourcePath, std::vector<unsigned char>& outData) {
        if (!initialized || !pakFile.is_open()) {
            std::cerr << "[ResourcePak] ERROR: PAK not initialized" << std::endl;
            return false;
        }

        // Find entry
        auto it = entries.find(resourcePath);
        if (it == entries.end()) {
            std::cerr << "[ResourcePak] ERROR: File not found in PAK: " << resourcePath << std::endl;
            return false;
        }

        const PakEntry& entry = it->second;
        outData.resize(entry.size);

        // Read from PAK
        pakFile.seekg(entry.offset, std::ios::beg);
        pakFile.read(reinterpret_cast<char*>(outData.data()), entry.size);

        if (!pakFile.good() && !pakFile.eof()) {
            std::cerr << "[ResourcePak] ERROR: Failed to read file: " << resourcePath << std::endl;
            return false;
        }

        std::cout << "[ResourcePak] Loaded: " << resourcePath << " (" << entry.size << " bytes)" << std::endl;
        return true;
    }

    /**
     * Get file data as string (for text files)
     */
    static bool LoadFileAsString(const std::string& resourcePath, std::string& outData) {
        std::vector<unsigned char> data;
        if (!LoadFile(resourcePath, data)) {
            return false;
        }
        outData = std::string(data.begin(), data.end());
        return true;
    }

    /**
     * Check if file exists in PAK
     */
    static bool FileExists(const std::string& resourcePath) {
        if (!initialized || !pakFile.is_open()) {
            return false;
        }
        return entries.find(resourcePath) != entries.end();
    }

    /**
     * List all files in PAK (for debugging)
     */
    static void ListFiles() {
        if (!initialized || !pakFile.is_open()) {
            std::cerr << "[ResourcePak] PAK not initialized" << std::endl;
            return;
        }

        std::cout << "[ResourcePak] Files in PAK (" << entries.size() << " entries):" << std::endl;
        for (const auto& entry : entries) {
            std::cout << "  - " << entry.first << " (" << entry.second.size << " bytes)" << std::endl;
        }
    }

private:
    /**
     * Read PAK header and build file index
     */
    static bool ReadPakHeader() {
        if (!pakFile.is_open()) return false;

        // Read signature
        uint32_t signature;
        pakFile.read(reinterpret_cast<char*>(&signature), sizeof(uint32_t));
        
        if (signature != PAK_SIGNATURE) {
            std::cerr << "[ResourcePak] ERROR: Invalid PAK signature" << std::endl;
            return false;
        }

        // Read number of files
        uint32_t numFiles;
        pakFile.read(reinterpret_cast<char*>(&numFiles), sizeof(uint32_t));

        std::cout << "[ResourcePak] Reading " << numFiles << " entries from PAK" << std::endl;

        // Read file entries
        for (uint32_t i = 0; i < numFiles; i++) {
            // Read filename length
            uint16_t pathLength;
            pakFile.read(reinterpret_cast<char*>(&pathLength), sizeof(uint16_t));

            // Read filename
            std::vector<char> pathBuffer(pathLength);
            pakFile.read(pathBuffer.data(), pathLength);
            std::string path(pathBuffer.begin(), pathBuffer.end());

            // Read offset and size
            uint32_t offset, size;
            pakFile.read(reinterpret_cast<char*>(&offset), sizeof(uint32_t));
            pakFile.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));

            PakEntry entry;
            entry.path = path;
            entry.offset = offset;
            entry.size = size;
            
            entries[path] = entry;
            std::cout << "[ResourcePak] Indexed: " << path << " (offset: " << offset << ", size: " << size << ")" << std::endl;
        }

        return true;
    }
};