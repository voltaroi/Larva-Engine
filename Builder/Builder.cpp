#include "Builder.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <windows.h>

namespace fs = std::filesystem;

Builder::Builder() : m_compilerType(CompilerType::UNKNOWN), m_verbose(false) {
    m_compilerType = detectCompiler();
}

CompilerType Builder::detectCompiler() {
    log("Détection du compilateur...");
    
    // Essayer GCC en premier (mingw64)
    if (findGCC(m_compilerPath)) {
        log("✓ GCC détecté: " + m_compilerPath);
        return CompilerType::GCC;
    }
    
    // Essayer Clang
    if (findClang(m_compilerPath)) {
        log("✓ Clang détecté: " + m_compilerPath);
        return CompilerType::CLANG;
    }
    
    // Essayer MSVC
    if (findMSVC(m_compilerPath)) {
        log("✓ MSVC détecté: " + m_compilerPath);
        return CompilerType::MSVC;
    }
    
    error("✗ Aucun compilateur trouvé!");
    return CompilerType::UNKNOWN;
}

bool Builder::findGCC(std::string& outPath) {
    // Essayer d'exécuter g++ --version
    FILE* pipe = _popen("where g++", "r");
    if (!pipe) return false;
    
    char buffer[256];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    _pclose(pipe);
    
    if (!result.empty()) {
        // Prendre la première ligne (premier chemin trouvé)
        size_t newlinePos = result.find('\n');
        outPath = (newlinePos != std::string::npos) ? result.substr(0, newlinePos) : result;
        // Nettoyer les espaces
        while (!outPath.empty() && (outPath.back() == '\r' || outPath.back() == '\n' || outPath.back() == ' ')) {
            outPath.pop_back();
        }
        return !outPath.empty();
    }
    return false;
}

bool Builder::findClang(std::string& outPath) {
    FILE* pipe = _popen("where clang++", "r");
    if (!pipe) return false;
    
    char buffer[256];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    _pclose(pipe);
    
    if (!result.empty()) {
        size_t newlinePos = result.find('\n');
        outPath = (newlinePos != std::string::npos) ? result.substr(0, newlinePos) : result;
        while (!outPath.empty() && (outPath.back() == '\r' || outPath.back() == '\n' || outPath.back() == ' ')) {
            outPath.pop_back();
        }
        return !outPath.empty();
    }
    return false;
}

bool Builder::findMSVC(std::string& outPath) {
    FILE* pipe = _popen("where cl", "r");
    if (!pipe) return false;
    
    char buffer[256];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    _pclose(pipe);
    
    if (!result.empty()) {
        size_t newlinePos = result.find('\n');
        outPath = (newlinePos != std::string::npos) ? result.substr(0, newlinePos) : result;
        while (!outPath.empty() && (outPath.back() == '\r' || outPath.back() == '\n' || outPath.back() == ' ')) {
            outPath.pop_back();
        }
        return !outPath.empty();
    }
    return false;
}

bool Builder::build(const ProjectConfig& config) {
    log("=== Build de " + config.name + " ===");
    
    if (m_compilerType == CompilerType::UNKNOWN) {
        error("Aucun compilateur disponible!");
        return false;
    }
    
    // Créer les dossiers de sortie
    createDirectory(config.objectDir);
    createDirectory(config.outputDir);
    
    // Compiler chaque fichier source
    std::vector<std::string> objectFiles;
    int compiledCount = 0;
    int skippedCount = 0;
    
    for (const auto& sourceFile : config.sourceFiles) {
        // Générer le nom du fichier objet
        fs::path sourcePath(sourceFile);
        std::string objName = sourcePath.stem().string();
        
        // Pour éviter les collisions de noms, inclure le chemin relatif
        std::string relativePath = sourceFile;
        std::replace(relativePath.begin(), relativePath.end(), '\\', '_');
        std::replace(relativePath.begin(), relativePath.end(), '/', '_');
        std::replace(relativePath.begin(), relativePath.end(), ':', '_');
        
        std::string objectFile = config.objectDir + "/" + relativePath;
        if (m_compilerType == CompilerType::MSVC) {
            objectFile += ".obj";
        } else {
            objectFile += ".o";
        }
        
        objectFiles.push_back(objectFile);
        
        // Vérifier si recompilation nécessaire
        if (needsRecompile(sourceFile, objectFile)) {
            log("[COMPILE] " + sourceFile);
            if (!compileFile(sourceFile, objectFile, config)) {
                error("Échec de compilation: " + sourceFile);
                return false;
            }
            compiledCount++;
        } else {
            if (m_verbose) {
                log("[SKIP   ] " + sourceFile);
            }
            skippedCount++;
        }
    }
    
    log("Fichiers compilés: " + std::to_string(compiledCount) + 
        ", ignorés: " + std::to_string(skippedCount));
    
    // Linkage
    std::string outputFile = config.outputDir + "/" + config.outputName;
    if (m_compilerType == CompilerType::MSVC) {
        outputFile += ".exe";
    } else {
        outputFile += ".exe";
    }
    
    log("[LINK   ] " + outputFile);
    if (!linkObjects(objectFiles, outputFile, config)) {
        error("Échec du linkage!");
        return false;
    }
    
    log("✓ Build réussi: " + outputFile);
    return true;
}

bool Builder::compileFile(const std::string& sourceFile, 
                          const std::string& objectFile,
                          const ProjectConfig& config) {
    
    // Créer le dossier de l'objet si nécessaire
    fs::path objPath(objectFile);
    if (objPath.has_parent_path()) {
        createDirectory(objPath.parent_path().string());
    }
    
    std::string command = buildCompileCommand(sourceFile, objectFile, config);
    return executeCommand(command);
}

bool Builder::linkObjects(const std::vector<std::string>& objectFiles,
                          const std::string& outputFile,
                          const ProjectConfig& config) {
    
    std::string command = buildLinkCommand(objectFiles, outputFile, config);
    return executeCommand(command);
}

std::string Builder::buildCompileCommand(const std::string& sourceFile,
                                         const std::string& objectFile,
                                         const ProjectConfig& config) {
    std::ostringstream cmd;
    
    if (m_compilerType == CompilerType::GCC || m_compilerType == CompilerType::CLANG) {
        cmd << "g++ -c \"" << sourceFile << "\" -o \"" << objectFile << "\"";
        cmd << " -std=" << config.cppStandard;
        
        // Includes
        for (const auto& inc : config.includeDirs) {
            cmd << " -I\"" << inc << "\"";
        }
        
        // Defines
        for (const auto& def : config.defines) {
            cmd << " -D" << def;
        }
        
        // Options de build
        if (config.buildType == BuildType::DEBUG) {
            cmd << " -g -O0";
        } else {
            cmd << " -O2";
        }
        
    } else if (m_compilerType == CompilerType::MSVC) {
        cmd << "cl /c \"" << sourceFile << "\" /Fo\"" << objectFile << "\"";
        cmd << " /std:" << config.cppStandard;
        cmd << " /EHsc /nologo";
        
        // Includes
        for (const auto& inc : config.includeDirs) {
            cmd << " /I\"" << inc << "\"";
        }
        
        // Defines
        for (const auto& def : config.defines) {
            cmd << " /D" << def;
        }
        
        if (config.buildType == BuildType::DEBUG) {
            cmd << " /Od /Zi";
        } else {
            cmd << " /O2";
        }
    }
    
    return cmd.str();
}

std::string Builder::buildLinkCommand(const std::vector<std::string>& objectFiles,
                                      const std::string& outputFile,
                                      const ProjectConfig& config) {
    std::ostringstream cmd;
    
    if (m_compilerType == CompilerType::GCC || m_compilerType == CompilerType::CLANG) {
        cmd << "g++";
        
        // Fichiers objets
        for (const auto& obj : objectFiles) {
            cmd << " \"" << obj << "\"";
        }
        
        cmd << " -o \"" << outputFile << "\"";
        
        // Library dirs
        for (const auto& libDir : config.libraryDirs) {
            cmd << " -L\"" << libDir << "\"";
        }
        
        // Libraries
        for (const auto& lib : config.libraries) {
            cmd << " -l" << lib;
        }
        
        // Type d'application
        if (!config.isConsoleApp) {
            cmd << " -mwindows";
        } else {
            cmd << " -mconsole";
        }
        
        // Linking statique
        if (config.staticLink) {
            cmd << " -static";
        }
        
    } else if (m_compilerType == CompilerType::MSVC) {
        cmd << "link /NOLOGO";
        
        // Fichiers objets
        for (const auto& obj : objectFiles) {
            cmd << " \"" << obj << "\"";
        }
        
        cmd << " /OUT:\"" << outputFile << "\"";
        
        // Library dirs
        for (const auto& libDir : config.libraryDirs) {
            cmd << " /LIBPATH:\"" << libDir << "\"";
        }
        
        // Libraries
        for (const auto& lib : config.libraries) {
            cmd << " " << lib << ".lib";
        }
        
        if (!config.isConsoleApp) {
            cmd << " /SUBSYSTEM:WINDOWS";
        } else {
            cmd << " /SUBSYSTEM:CONSOLE";
        }
    }
    
    return cmd.str();
}

bool Builder::needsRecompile(const std::string& sourceFile, 
                             const std::string& objectFile) {
    // Si l'objet n'existe pas, il faut compiler
    if (!fs::exists(objectFile)) {
        return true;
    }
    
    // Comparer les timestamps
    long long sourceTime = getFileTimestamp(sourceFile);
    long long objectTime = getFileTimestamp(objectFile);
    
    return sourceTime > objectTime;
}

long long Builder::getFileTimestamp(const std::string& filePath) {
    try {
        auto ftime = fs::last_write_time(filePath);
        return ftime.time_since_epoch().count();
    } catch (...) {
        return 0;
    }
}

bool Builder::executeCommand(const std::string& command) {
    if (m_verbose) {
        log("CMD: " + command);
    }
    
    int result = system(command.c_str());
    return result == 0;
}

void Builder::createDirectory(const std::string& path) {
    try {
        fs::create_directories(path);
    } catch (const std::exception& e) {
        error("Erreur création dossier: " + std::string(e.what()));
    }
}

bool Builder::clean(const ProjectConfig& config) {
    log("=== Nettoyage de " + config.name + " ===");
    
    try {
        if (fs::exists(config.objectDir)) {
            fs::remove_all(config.objectDir);
            log("✓ Dossier objets supprimé");
        }
        
        std::string outputFile = config.outputDir + "/" + config.outputName + ".exe";
        if (fs::exists(outputFile)) {
            fs::remove(outputFile);
            log("✓ Executable supprimé");
        }
    } catch (const std::exception& e) {
        error("Erreur nettoyage: " + std::string(e.what()));
        return false;
    }
    
    return true;
}

bool Builder::rebuild(const ProjectConfig& config) {
    clean(config);
    return build(config);
}

void Builder::log(const std::string& message) {
    std::cout << message << std::endl;
}

void Builder::error(const std::string& message) {
    std::cerr << "ERREUR: " << message << std::endl;
}
