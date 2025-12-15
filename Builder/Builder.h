#pragma once

#include "ProjectConfig.h"
#include <string>
#include <vector>
#include <map>

class Builder {
public:
    Builder();
    
    // Détecte automatiquement le compilateur disponible
    CompilerType detectCompiler();
    
    // Compile le projet selon la configuration
    bool build(const ProjectConfig& config);
    
    // Nettoie les fichiers générés
    bool clean(const ProjectConfig& config);
    
    // Reconstruction complète
    bool rebuild(const ProjectConfig& config);
    
    void setVerbose(bool verbose) { m_verbose = verbose; }
    void setCompiler(CompilerType type) { m_compilerType = type; }
    
private:
    CompilerType m_compilerType;
    bool m_verbose;
    
    // Chemins des compilateurs détectés
    std::string m_compilerPath;
    std::string m_linkerPath;
    
    // Cache de timestamps pour compilation incrémentale
    std::map<std::string, long long> m_fileTimestamps;
    
    // Détection des compilateurs
    bool findMSVC(std::string& outPath);
    bool findGCC(std::string& outPath);
    bool findClang(std::string& outPath);
    
    // Compilation
    bool compileFile(const std::string& sourceFile, 
                     const std::string& objectFile,
                     const ProjectConfig& config);
    
    bool linkObjects(const std::vector<std::string>& objectFiles,
                     const std::string& outputFile,
                     const ProjectConfig& config);
    
    // Utilitaires
    std::string buildCompileCommand(const std::string& sourceFile,
                                    const std::string& objectFile,
                                    const ProjectConfig& config);
    
    std::string buildLinkCommand(const std::vector<std::string>& objectFiles,
                                const std::string& outputFile,
                                const ProjectConfig& config);
    
    bool needsRecompile(const std::string& sourceFile, 
                       const std::string& objectFile);
    
    long long getFileTimestamp(const std::string& filePath);
    bool executeCommand(const std::string& command);
    void createDirectory(const std::string& path);
    void log(const std::string& message);
    void error(const std::string& message);
};
