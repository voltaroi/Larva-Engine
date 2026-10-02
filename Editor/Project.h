#pragma once
#ifndef __EDITOR_PROJECT__
#define __EDITOR_PROJECT__
#include <string>
#include <vector>

// Accès aux projets du moteur (dossier projects/) et à leurs fichiers
namespace project
{
    struct Info
    {
        std::string name;
        std::string dir;       // chemin absolu du projet
        std::string assetsDir; // chemin absolu de Assets
        int levelCount = 0;
    };

    // Dossier racine du moteur (contient Engine/ et projects/), cherché depuis l'exécutable puis le dossier courant
    std::string findEngineRoot();

    std::vector<Info> listProjects(const std::string &root);

    // Chemins relatifs aux Assets, triés : "Levels/Main.lvl"...
    std::vector<std::string> listLevels(const std::string &assetsDir);
    std::vector<std::string> listModels(const std::string &assetsDir);

    // Copie les shaders du moteur dans Assets/Shaders s'ils manquent (comme build_client.bat)
    void ensureShaders(const std::string &root, const std::string &assetsDir);

    // Nom de projet ou de niveau valide : lettres, chiffres, _ (1 à 48 caractères)
    bool isValidName(const std::string &name);

    // Lance create_project.bat ; message contient la sortie en cas d'échec
    bool createProject(const std::string &root, const std::string &name, std::string &message);

    // Niveau de départ : sol, cube et point d'apparition
    bool createLevel(const std::string &assetsDir, const std::string &name, std::string &relativePath, std::string &message);
}

#endif
