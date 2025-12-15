#include "Builder.h"
#include "ProjectConfig.h"
#include <iostream>
#include <string>
#include <windows.h>
#include <clocale>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options] <config.json>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -v, --verbose    Mode verbeux\n";
    std::cout << "  -r, --rebuild    Reconstruction complete\n";
    std::cout << "  -c, --clean      Nettoyer les fichiers generes\n";
        std::cout << "  -d, --debug      Build en mode debug\n";
        std::cout << "  --release        Build en mode release\n";
        std::cout << "  -j, --jobs N     Nombre de threads de compilation (defaut: auto)\n";
        std::cout << "  --no-progress    Desactiver la barre de progression\n";
    std::cout << "  --msvc           Forcer l'utilisation de MSVC\n";
    std::cout << "  --gcc            Forcer l'utilisation de GCC\n";
    std::cout << "  --clang          Forcer l'utilisation de Clang\n";
    std::cout << "\nExemples:\n";
    std::cout << "  " << programName << " engine_config.json\n";
        std::cout << "  " << programName << " -r -j 8 game_config.json\n";
        std::cout << "  " << programName << " --debug --verbose game_config.json\n";
        std::cout << "  " << programName << " --clean server_config.json\n";
}

int main(int argc, char* argv[]) {
    // Activer l'UTF-8 pour les caracteres accentues dans la console Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::setlocale(LC_ALL, "");

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    bool verbose = false;
    bool rebuild = false;
    bool clean = false;
        bool debugOverride = false;
        bool releaseOverride = false;
        int jobs = 0;
        bool showProgress = true;
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
            } else if (arg == "-d" || arg == "--debug") {
                debugOverride = true;
            } else if (arg == "--release") {
                releaseOverride = true;
            } else if (arg == "-j" || arg == "--jobs") {
                if (i + 1 < argc) {
                    jobs = std::atoi(argv[i + 1]);
                    i++;
                }
            } else if (arg == "--no-progress") {
                showProgress = false;
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
        std::cerr << "Erreur: aucun fichier de configuration specifie" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Charger la configuration
    ProjectConfig config;
    if (!config.loadFromJson(configFile)) {
        std::cerr << "Erreur: impossible de charger " << configFile << std::endl;
        return 1;
    }
    
        // Appliquer les overrides CLI
        if (debugOverride) {
            config.buildType = BuildType::DEBUG;
        } else if (releaseOverride) {
            config.buildType = BuildType::RELEASE;
        }
    
    // Créer le builder
    Builder builder;
    builder.setVerbose(verbose);
        builder.setShowProgress(showProgress);
    
        if (jobs > 0) {
            builder.setJobs(jobs);
        }
    
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
