#include "Builder.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <windows.h>

namespace fs = std::filesystem;

static bool isLinuxTarget(const ProjectConfig& config) {
    return config.targetPlatform == TargetPlatform::LINUX;
}

static std::string compilerPathForTarget(const ProjectConfig& config) {
    if (isLinuxTarget(config)) {
        return "Dependencies/Compiler/zig/zig-cxx.bat";
    }
    return "Dependencies/Compiler/clang/bin/clang++.exe";
}

static std::string outputPathForTarget(const ProjectConfig& config) {
    fs::path outputPath = fs::path(config.outputDir) / config.outputName;
    if (!isLinuxTarget(config)) {
        outputPath += ".exe";
    }
    return outputPath.generic_string();
}

// Simple wildcard matcher supporting '*' and '?'
static bool matchWildcard(const std::string& text, const std::string& pattern) {
    size_t t = 0, p = 0, star = std::string::npos, match = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++t; ++p;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++; match = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++match;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

static bool hasWildcard(const std::string& path) {
    return path.find('*') != std::string::npos || path.find('?') != std::string::npos;
}

static std::vector<std::string> expandWildcardPaths(const std::vector<std::string>& inputs) {
    std::vector<std::string> expanded;
    for (const auto& entry : inputs) {
        if (!hasWildcard(entry)) {
            expanded.push_back(entry);
            continue;
        }

        fs::path patternPath(entry);
        fs::path dir = patternPath.parent_path();
        if (dir.empty()) dir = fs::path(".");
        const std::string mask = patternPath.filename().string();

        try {
            if (!fs::exists(dir) || !fs::is_directory(dir)) {
                std::cerr << "[WARN] Dossier introuvable pour wildcard: " << dir.string() << std::endl;
                continue;
            }
            for (const auto& it : fs::directory_iterator(dir)) {
                if (it.is_regular_file() && matchWildcard(it.path().filename().string(), mask)) {
                    expanded.push_back(it.path().string());
                }
            }
            if (expanded.empty()) {
                std::cerr << "[WARN] Aucun fichier ne correspond a: " << entry << std::endl;
            }
        } catch (const std::exception& ex) {
            std::cerr << "[WARN] Erreur d'expansion du wildcard " << entry << ": " << ex.what() << std::endl;
        }
    }
    return expanded;
}

Builder::Builder() : m_compilerType(CompilerType::UNKNOWN), m_verbose(false), 
                            m_jobs(std::thread::hardware_concurrency()), m_showProgress(true) {
     if (m_jobs == 0) m_jobs = 4; // Fallback si détection échoue
    m_compilerType = detectCompiler();
}

CompilerType Builder::detectCompiler() {
    info("Detection du compilateur...");
    // Forcer Clang/LLVM comme compilateur par défaut
    m_compilerPath = "Dependencies/Compiler/clang/bin/clang++.exe";
    success("[OK] Clang force: " + m_compilerPath);
    return CompilerType::CLANG;
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
    auto startTime = std::chrono::high_resolution_clock::now();
    
    info("=== Build de " + config.name + " ===");
    info("Threads de compilation: " + std::to_string(m_jobs));
    
    if (m_compilerType == CompilerType::UNKNOWN) {
        error("Aucun compilateur disponible!");
        return false;
    }

    if (isLinuxTarget(config)) {
        std::string targetCompiler = compilerPathForTarget(config);
        if (!fs::exists(targetCompiler)) {
            error("Le wrapper zig-cxx.bat Linux n'est pas installe. Lance bootstrap_builder.bat.");
            return false;
        }
        info("Target platform: Linux (cross-compilation via Zig)");
    }
    
    // Créer les dossiers de sortie
    createDirectory(config.objectDir);
    createDirectory(config.outputDir);
    
    // Préparer la liste des fichiers à compiler
    std::vector<std::string> sourceList = expandWildcardPaths(config.sourceFiles);
    std::vector<std::string> objectFiles;
    std::vector<std::pair<std::string, std::string>> filesToCompile;
    int compiledCount = 0;
    int skippedCount = 0;
    
    for (const auto& sourceFile : sourceList) {
        // Skip non-C/C++ source files (e.g., headers matched by wildcards)
        fs::path srcPath(sourceFile);
        std::string ext = srcPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".cpp" && ext != ".cc" && ext != ".cxx" && ext != ".c++" && ext != ".c") {
            continue;
        }
        // Générer le chemin relatif du fichier source par rapport à la racine du projet
        fs::path rel = fs::relative(sourceFile, ".");
        fs::path objPath = fs::path(config.objectDir) / rel;
        objPath.replace_extension(m_compilerType == CompilerType::MSVC ? ".obj" : ".o");
        createDirectory(objPath.parent_path().string());
        std::string objectFile = objPath.generic_string();
        objectFiles.push_back(objectFile);
        // Vérifier si recompilation nécessaire
        if (needsRecompile(sourceFile, objectFile) || anyDependencyChanged(sourceFile, objectFile)) {
            // Normaliser le chemin source pour la ligne de commande
            fs::path srcPath(sourceFile);
            filesToCompile.push_back({srcPath.generic_string(), objectFile});
        } else {
            if (m_verbose) {
                log("[SKIP   ] " + sourceFile);
            }
            skippedCount++;
        }
    }
    
    // Compilation parallèle
    if (!filesToCompile.empty()) {
        int totalFiles = filesToCompile.size();
        int completed = 0;
        bool hasError = false;
        
        std::vector<std::thread> threads;
        size_t fileIndex = 0;
        
        for (int i = 0; i < m_jobs && fileIndex < filesToCompile.size(); ++i) {
            threads.emplace_back([&, i]() {
                while (true) {
                    size_t currentIndex;
                    
                    // Obtenir le prochain fichier à compiler
                    {
                        std::lock_guard<std::mutex> lock(m_progressMutex);
                        if (fileIndex >= filesToCompile.size() || hasError) {
                            break;
                        }
                        currentIndex = fileIndex++;
                    }
                    
                    auto& [src, obj] = filesToCompile[currentIndex];
                    
                    if (m_verbose) {
                        std::lock_guard<std::mutex> lock(m_outputMutex);
                        log("[COMPILE] " + src);
                    }
                    
                    if (!compileFile(src, obj, config)) {
                        std::lock_guard<std::mutex> lock(m_outputMutex);
                        error("Echec de compilation: " + src);
                        hasError = true;
                        break;
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(m_progressMutex);
                        completed++;
                        if (m_showProgress) {
                            showProgressBar(completed, totalFiles);
                        }
                    }
                }
            });
        }
        
        // Attendre la fin de tous les threads
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        
        if (hasError) {
            return false;
        }
        
        compiledCount = totalFiles;
    }
    
    info("Fichiers compiles: " + std::to_string(compiledCount) + 
        ", ignores: " + std::to_string(skippedCount));
    
    // Vérification obligatoire : bloquer le link si aucun objet valide
    if (objectFiles.empty()) {
        error("Aucun fichier objet genere, linkage annule.");
        return false;
    }

    // Vérifier que les fichiers objets existent vraiment
    bool hasValidObjects = false;
    for (const auto& obj : objectFiles) {
        if (fs::exists(obj)) {
            hasValidObjects = true;
            break;
        }
    }

    if (!hasValidObjects) {
        error("Aucun fichier objet valide trouve sur disque, linkage annule.");
        return false;
    }

    // Linkage
    std::string outputFile = outputPathForTarget(config);

    info("[LINK   ] " + outputFile);
    if (!linkObjects(objectFiles, outputFile, config)) {
        error("Echec du linkage!");
        return false;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    double seconds = duration.count() / 1000.0;
    
    success("[OK] Build reussi: " + outputFile);
    success("[TIME] " + std::to_string(seconds) + "s");
    
    return true;
}

bool Builder::compileFile(const std::string& sourceFile, 
                          const std::string& objectFile,
                          const ProjectConfig& config) {
    fs::path objPath(objectFile);
    if (objPath.has_parent_path()) createDirectory(objPath.parent_path().string());

    // Utiliser un fichier de réponse unique par compilation
    std::string rspFile = objPath.string() + ".rsp";
    std::ofstream rsp(rspFile, std::ios::out | std::ios::trunc);
    if (!rsp) {
        error("Impossible de créer le fichier de réponse pour la compilation.");
        return false;
    }
    // Arguments principaux (tout sur une seule ligne, options et valeurs ensemble)
    fs::path absSrc = fs::absolute(fs::path(sourceFile));
    fs::path absObj = fs::absolute(objPath);
    rsp << "-c\n";
    rsp << '"' << absSrc.generic_string() << '"' << "\n";
    rsp << "-o\n";
    rsp << '"' << absObj.generic_string() << '"' << "\n";
    rsp << "-std=" << config.cppStandard << "\n";
    // Includes
    for (const auto& inc : config.includeDirs) {
        fs::path absInc = fs::absolute(fs::path(inc));
        rsp << "-I\n";
        rsp << '"' << absInc.generic_string() << '"' << "\n";
    }
    // Defines
    for (const auto& def : config.defines) {
        rsp << "-D" << def << "\n";
    }
    // Options de build
    if (config.buildType == BuildType::DEBUG) {
        rsp << "-g\n";
        rsp << "-O0\n";
    } else {
        rsp << "-O2\n";
    }
    rsp.close();
    std::string command = '"' + fs::absolute(fs::path(compilerPathForTarget(config))).make_preferred().string() + '"' + " @" + fs::absolute(fs::path(rspFile)).make_preferred().string();
    std::cout << "[DEBUG CMD] " << command << std::endl;
    bool result = executeCommand(command);
    return result;
}

bool Builder::linkObjects(const std::vector<std::string>& objectFiles,
                          const std::string& outputFile,
                          const ProjectConfig& config) {
    
    // Utiliser un fichier de réponse unique par linkage
    fs::path outPath(outputFile);
    std::string rspFile = outPath.stem().string() + ".link.rsp";
    std::ofstream rsp(rspFile);
    if (!rsp) {
        error("Impossible de créer le fichier de réponse pour le linkage.");
        return false;
    }
    // Fichiers objets
    for (const auto& obj : objectFiles) {
        fs::path absObj = fs::absolute(fs::path(obj));
        rsp << '"' << absObj.generic_string() << '"' << "\n";
    }
    // Output
    fs::path absOut = fs::absolute(fs::path(outputFile));
    rsp << "-o\n";
    rsp << '"' << absOut.generic_string() << '"' << "\n";
    if (!isLinuxTarget(config)) {
        // Forcer l'utilisation du linker LLVM pour le build Windows
        rsp << "-fuse-ld=lld\n";
    }
    // Library dirs
    for (const auto& libDir : config.libraryDirs) {
        fs::path absLib = fs::absolute(fs::path(libDir));
        rsp << "-L\n";
        rsp << '"' << absLib.generic_string() << '"' << "\n";
    }
    // Libraries
    for (const auto& lib : config.libraries) {
        if (lib == "mingw32") continue; // Ne pas linker mingw32.lib
        rsp << "-l" << lib << "\n";
    }
    // Type d'application Windows uniquement
    if (!isLinuxTarget(config)) {
        if (!config.isConsoleApp) {
            rsp << "-mwindows\n";
        } else {
            rsp << "-mconsole\n";
        }
    }
    // Linking statique
    if (config.staticLink) {
        rsp << "-static\n";
    }
    rsp.close();
    std::string command = '"' + fs::absolute(fs::path(compilerPathForTarget(config))).make_preferred().string() + '"' + " @" + fs::absolute(fs::path(rspFile)).make_preferred().string();
    bool result = executeCommand(command);
    std::remove(rspFile.c_str());
    return result;
}

std::string Builder::buildCompileCommand(const std::string& sourceFile,
                                         const std::string& objectFile,
                                         const ProjectConfig& config) {
    std::ostringstream cmd;
    std::string compilerPath = compilerPathForTarget(config);
    
    if (m_compilerType == CompilerType::GCC || m_compilerType == CompilerType::CLANG) {
        // Utiliser des chemins normalisés pour la ligne de commande
        fs::path srcPath(sourceFile);
        fs::path objPath(objectFile);
        cmd << "\"" << compilerPath << "\" -c \"" 
            << srcPath.generic_string() << "\" -o \"" << objPath.generic_string() << "\"";
        cmd << " -std=" << config.cppStandard;
        
        // Includes
        for (const auto& inc : config.includeDirs) {
            fs::path incPath(inc);
            cmd << " -I\"" << incPath.generic_string() << "\"";
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
    std::string compilerPath = compilerPathForTarget(config);
    
    if (m_compilerType == CompilerType::GCC || m_compilerType == CompilerType::CLANG) {
        cmd << "\"" << compilerPath << "\"";
        // Fichiers objets
        for (const auto& obj : objectFiles) {
            fs::path objPath(obj);
            cmd << " \"" << objPath.generic_string() << "\"";
        }
        fs::path outPath(outputFile);
        cmd << " -o \"" << outPath.generic_string() << "\"";
        // Library dirs
        for (const auto& libDir : config.libraryDirs) {
            fs::path libPath(libDir);
            cmd << " -L\"" << libPath.generic_string() << "\"";
        }
        
        // Libraries
        for (const auto& lib : config.libraries) {
            cmd << " -l" << lib;
        }
        
        // Type d'application
        if (!isLinuxTarget(config)) {
            if (!config.isConsoleApp) {
                cmd << " -mwindows";
            } else {
                cmd << " -mconsole";
            }
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
    // Affiche toujours la commande pour le linkage (et temporairement pour tout)
    std::cout << "[CMD] " << command << std::endl;
    int result = system(command.c_str());
    return result == 0;
}

void Builder::createDirectory(const std::string& path) {
    try {
        fs::create_directories(path);
    } catch (const std::exception& e) {
        error("Erreur creation dossier: " + std::string(e.what()));
    }
}

bool Builder::clean(const ProjectConfig& config) {
    info("=== Nettoyage de " + config.name + " ===");
    
    try {
        if (fs::exists(config.objectDir)) {
            fs::remove_all(config.objectDir);
            success("[OK] Dossier objets supprime");
        }
        
        std::string outputFile = outputPathForTarget(config);
        if (fs::exists(outputFile)) {
            fs::remove(outputFile);
            success("[OK] Executable supprime");
        }

        std::string windowsOutputFile = config.outputDir + "/" + config.outputName + ".exe";
        if (windowsOutputFile != outputFile && fs::exists(windowsOutputFile)) {
            fs::remove(windowsOutputFile);
            success("[OK] Ancien executable Windows supprime");
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
        setConsoleColor(12); // Rouge
    std::cerr << "ERREUR: " << message << std::endl;
        resetConsoleColor();
    }

    void Builder::success(const std::string& message) {
        setConsoleColor(10); // Vert
        std::cout << message << std::endl;
        resetConsoleColor();
    }

    void Builder::info(const std::string& message) {
        setConsoleColor(11); // Cyan
        std::cout << message << std::endl;
        resetConsoleColor();
    }

    void Builder::warning(const std::string& message) {
        setConsoleColor(14); // Jaune
        std::cout << "ATTENTION: " << message << std::endl;
        resetConsoleColor();
    }

    void Builder::setConsoleColor(int color) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, color);
    }

    void Builder::resetConsoleColor() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, 7); // Blanc par défaut
    }

    void Builder::scanDependencies(const std::string& sourceFile, std::vector<std::string>& deps) {
        std::ifstream file(sourceFile);
        if (!file.is_open()) return;
    
        std::string line;
        while (std::getline(file, line)) {
            // Chercher les #include "..."
            size_t includePos = line.find("#include");
            if (includePos != std::string::npos) {
                size_t quoteStart = line.find('"', includePos);
                if (quoteStart != std::string::npos) {
                    size_t quoteEnd = line.find('"', quoteStart + 1);
                    if (quoteEnd != std::string::npos) {
                        std::string headerFile = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    
                        // Construire le chemin complet
                        fs::path sourcePath(sourceFile);
                        fs::path headerPath = sourcePath.parent_path() / headerFile;
                    
                        if (fs::exists(headerPath)) {
                            deps.push_back(headerPath.string());
                        }
                    }
                }
            }
        }
    }

    bool Builder::anyDependencyChanged(const std::string& sourceFile, const std::string& objectFile) {
        if (!fs::exists(objectFile)) {
            return true;
        }
    
        // Scanner les dépendances si pas déjà en cache
        if (m_dependencies.find(sourceFile) == m_dependencies.end()) {
            std::vector<std::string> deps;
            scanDependencies(sourceFile, deps);
            m_dependencies[sourceFile] = deps;
        }
    
        long long objectTime = getFileTimestamp(objectFile);
    
        // Vérifier si un header a changé
        for (const auto& dep : m_dependencies[sourceFile]) {
            long long depTime = getFileTimestamp(dep);
            if (depTime > objectTime) {
                return true;
            }
        }
    
        return false;
    }

    void Builder::showProgressBar(int current, int total) {
        // Barres ASCII pour compatibilité des terminaux et sortie sérialisée
        std::lock_guard<std::mutex> lock(m_outputMutex);
        const int barWidth = 40;
        float progress = static_cast<float>(current) / total;
        int pos = static_cast<int>(barWidth * progress);

        std::cout << "\r[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << '#';
            else std::cout << '.';
        }
        std::cout << "] " << int(progress * 100.0f) << "% (" << current << "/" << total << ")   ";
        std::cout.flush();

        if (current == total) {
            std::cout << std::endl;
        }
}
