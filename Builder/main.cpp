#include "Builder.h"
#include "ProjectConfig.h"
#include <iostream>
#include <string>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options] <config.json>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -v, --verbose    Mode verbeux\n";
    std::cout << "  -r, --rebuild    Reconstruction complète\n";
    std::cout << "  -c, --clean      Nettoyer les fichiers générés\n";
    std::cout << "  --msvc           Forcer l'utilisation de MSVC\n";
    std::cout << "  --gcc            Forcer l'utilisation de GCC\n";
    std::cout << "  --clang          Forcer l'utilisation de Clang\n";
    std::cout << "\nExemples:\n";
    std::cout << "  " << programName << " engine_config.json\n";
    std::cout << "  " << programName << " -r game_config.json\n";
    std::cout << "  " << programName << " --clean server_config.json\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    bool verbose = false;
    bool rebuild = false;
    bool clean = false;
    CompilerType forcedCompiler = CompilerType::UNKNOWN;
    std::string configFile;
    
    // Parser les arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-r" || arg == "--rebuild") {
            rebuild = true;
        } else if (arg == "-c" || arg == "--clean") {
            clean = true;
        } else if (arg == "--msvc") {
            forcedCompiler = CompilerType::MSVC;
        } else if (arg == "--gcc") {
            forcedCompiler = CompilerType::GCC;
        } else if (arg == "--clang") {
            forcedCompiler = CompilerType::CLANG;
        } else if (arg[0] != '-') {
            configFile = arg;
        } else {
            std::cerr << "Option inconnue: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (configFile.empty()) {
        std::cerr << "Erreur: aucun fichier de configuration spécifié" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Charger la configuration
    ProjectConfig config;
    if (!config.loadFromJson(configFile)) {
        std::cerr << "Erreur: impossible de charger " << configFile << std::endl;
        return 1;
    }
    
    // Créer le builder
    Builder builder;
    builder.setVerbose(verbose);
    
    if (forcedCompiler != CompilerType::UNKNOWN) {
        builder.setCompiler(forcedCompiler);
    }
    
    // Exécuter l'action demandée
    bool success = false;
    
    if (clean) {
        success = builder.clean(config);
    } else if (rebuild) {
        success = builder.rebuild(config);
    } else {
        success = builder.build(config);
    }
    
    return success ? 0 : 1;
}
