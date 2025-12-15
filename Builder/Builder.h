#pragma once

#include "ProjectConfig.h"
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <chrono>

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
     void setJobs(int jobs) { m_jobs = jobs; }
     void setShowProgress(bool show) { m_showProgress = show; }
    
private:
    CompilerType m_compilerType;
    bool m_verbose;
     int m_jobs;
     bool m_showProgress;
     std::mutex m_outputMutex;
     std::mutex m_progressMutex;
    
    // Chemins des compilateurs détectés
    std::string m_compilerPath;
    std::string m_linkerPath;
    
    // Cache de timestamps pour compilation incrémentale
    std::map<std::string, long long> m_fileTimestamps;
     std::map<std::string, std::vector<std::string>> m_dependencies;
    
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
    
     // Nouvelles fonctionnalités
     void scanDependencies(const std::string& sourceFile, std::vector<std::string>& deps);
     bool anyDependencyChanged(const std::string& sourceFile, const std::string& objectFile);
     void showProgressBar(int current, int total);
    
     // Logging avec couleurs
    void log(const std::string& message);
    void error(const std::string& message);
     void success(const std::string& message);
     void info(const std::string& message);
     void warning(const std::string& message);
    
     // Couleurs Windows
     void setConsoleColor(int color);
     void resetConsoleColor();
};
