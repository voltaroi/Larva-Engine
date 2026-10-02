#pragma once
#ifndef __EDITOR_BROWSER__
#define __EDITOR_BROWSER__
#include <string>
#include <vector>
#include "Project.h"

// Écrans d'accueil : choix du projet puis des niveaux du projet
class Browser
{
public:
    enum class Result
    {
        None,
        OpenLevel
    };

    void init(const std::string &root);
    Result frame(int screenWidth, int screenHeight);

    void showProjects();
    bool showLevels(const std::string &projectName);
    void setMessage(const std::string &text, bool error);

    const project::Info &selectedProject() const { return current; }
    const std::string &levelToOpen() const { return openPath; }

private:
    enum class Screen
    {
        Projects,
        Levels
    };
    Screen screen = Screen::Projects;
    std::string rootDir;
    std::vector<project::Info> projects;
    project::Info current;
    std::vector<std::string> levels;
    std::string openPath;
    std::string newName;
    std::string message;
    bool messageIsError = false;
    int confirmDelete = -1;

    Result drawProjects(float w, float h);
    Result drawLevels(float w, float h);
    void drawHeader(const std::string &title, const std::string &subtitle, float w);
    void drawMessage(float x, float y);
};

#endif
