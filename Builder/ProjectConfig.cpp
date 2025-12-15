#include "ProjectConfig.h"
#include <fstream>
#include <sstream>
#include <iostream>

// Simple JSON parser (basique, pas de lib externe)
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\",");
    return str.substr(first, (last - first + 1));
}

static std::vector<std::string> parseArray(const std::string& arrayContent) {
    std::vector<std::string> result;
    std::string current;
    bool inQuotes = false;
    
    for (char c : arrayContent) {
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            std::string trimmed = trim(current);
            if (!trimmed.empty()) {
                result.push_back(trimmed);
            }
            current.clear();
        } else {
            current += c;
        }
    }
    
    std::string trimmed = trim(current);
    if (!trimmed.empty()) {
        result.push_back(trimmed);
    }
    
    return result;
}

bool ProjectConfig::loadFromJson(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "Erreur: impossible d'ouvrir " << jsonPath << std::endl;
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // Parser simple ligne par ligne
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '{' || line[0] == '}') continue;
        
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string key = trim(line.substr(0, colonPos));
        std::string value = trim(line.substr(colonPos + 1));
        
        // Enlever la virgule finale si présente
        if (!value.empty() && value.back() == ',') {
            value.pop_back();
            value = trim(value);
        }
        
        if (key == "name") {
            name = value;
        } else if (key == "outputDir") {
            outputDir = value;
        } else if (key == "objectDir") {
            objectDir = value;
        } else if (key == "outputName") {
            outputName = value;
        } else if (key == "cppStandard") {
            cppStandard = value;
        } else if (key == "buildType") {
            buildType = (value == "debug" || value == "DEBUG") ? BuildType::DEBUG : BuildType::RELEASE;
        } else if (key == "isConsoleApp") {
            isConsoleApp = (value == "true");
        } else if (key == "staticLink") {
            staticLink = (value == "true");
        } else if (key == "sourceFiles") {
            // Lire jusqu'au ]
            std::string arrayContent;
            if (value.find('[') != std::string::npos) {
                arrayContent = value.substr(value.find('[') + 1);
                
                while (arrayContent.find(']') == std::string::npos) {
                    std::string nextLine;
                    if (!std::getline(stream, nextLine)) break;
                    arrayContent += nextLine;
                }
                
                size_t endPos = arrayContent.find(']');
                if (endPos != std::string::npos) {
                    arrayContent = arrayContent.substr(0, endPos);
                }
                
                sourceFiles = parseArray(arrayContent);
            }
        } else if (key == "includeDirs") {
            std::string arrayContent;
            if (value.find('[') != std::string::npos) {
                arrayContent = value.substr(value.find('[') + 1);
                
                while (arrayContent.find(']') == std::string::npos) {
                    std::string nextLine;
                    if (!std::getline(stream, nextLine)) break;
                    arrayContent += nextLine;
                }
                
                size_t endPos = arrayContent.find(']');
                if (endPos != std::string::npos) {
                    arrayContent = arrayContent.substr(0, endPos);
                }
                
                includeDirs = parseArray(arrayContent);
            }
        } else if (key == "libraryDirs") {
            std::string arrayContent;
            if (value.find('[') != std::string::npos) {
                arrayContent = value.substr(value.find('[') + 1);
                
                while (arrayContent.find(']') == std::string::npos) {
                    std::string nextLine;
                    if (!std::getline(stream, nextLine)) break;
                    arrayContent += nextLine;
                }
                
                size_t endPos = arrayContent.find(']');
                if (endPos != std::string::npos) {
                    arrayContent = arrayContent.substr(0, endPos);
                }
                
                libraryDirs = parseArray(arrayContent);
            }
        } else if (key == "libraries") {
            std::string arrayContent;
            if (value.find('[') != std::string::npos) {
                arrayContent = value.substr(value.find('[') + 1);
                
                while (arrayContent.find(']') == std::string::npos) {
                    std::string nextLine;
                    if (!std::getline(stream, nextLine)) break;
                    arrayContent += nextLine;
                }
                
                size_t endPos = arrayContent.find(']');
                if (endPos != std::string::npos) {
                    arrayContent = arrayContent.substr(0, endPos);
                }
                
                libraries = parseArray(arrayContent);
            }
        } else if (key == "defines") {
            std::string arrayContent;
            if (value.find('[') != std::string::npos) {
                arrayContent = value.substr(value.find('[') + 1);
                
                while (arrayContent.find(']') == std::string::npos) {
                    std::string nextLine;
                    if (!std::getline(stream, nextLine)) break;
                    arrayContent += nextLine;
                }
                
                size_t endPos = arrayContent.find(']');
                if (endPos != std::string::npos) {
                    arrayContent = arrayContent.substr(0, endPos);
                }
                
                defines = parseArray(arrayContent);
            }
        }
    }
    
    return !name.empty();
}
