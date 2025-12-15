#pragma once

#include <string>
#include <vector>

enum class CompilerType {
    MSVC,
    GCC,
    CLANG,
    UNKNOWN
};

enum class BuildType {
    DEBUG,
    RELEASE
};

struct ProjectConfig {
    std::string name;
    std::string outputDir;
    std::string objectDir;
    std::string outputName;
    
    std::vector<std::string> sourceFiles;
    std::vector<std::string> includeDirs;
    std::vector<std::string> libraryDirs;
    std::vector<std::string> libraries;
    std::vector<std::string> defines;
    
    std::string cppStandard = "c++17";
    BuildType buildType = BuildType::RELEASE;
    bool isConsoleApp = false;
    bool staticLink = true;
    
    // Chargement depuis JSON
    bool loadFromJson(const std::string& jsonPath);
};
