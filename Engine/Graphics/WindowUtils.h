#pragma once
#include <iostream>
#include <GL/freeglut.h>

// Mode d'affichage de la fenêtre du jeu
enum class WindowMode
{
    Windowed,   // fenêtre avec bordure, redimensionnable
    Borderless, // plein écran fenêtré : fenêtre sans bordure qui couvre l'écran (Alt+Tab instantané)
    Fullscreen, // plein écran exclusif : l'écran est pris par le jeu, qui se réduit quand il perd le focus
    Count
};

class WindowUtils
{
private:
    int FOV = 75;
    const char* name = "Larva-Engine";
    WindowMode mode = WindowMode::Borderless;
    int windowedW = 1280, windowedH = 720;
public:
    void setFullscreenBorderless(int screenWidth, int screenHeight);
    void setFullscreen();
    void setWindowed(int width, int height);
    // Passe dans un mode d'affichage (windowedW/H : taille de la fenêtre en mode fenêtré, 0 : la dernière demandée)
    void setWindowMode(WindowMode newMode, int windowedWidth = 0, int windowedHeight = 0);
    WindowMode getWindowMode() const { return mode; }
    static const char* windowModeName(WindowMode m); // "FENETRE", "PLEIN ECRAN FENETRE", "PLEIN ECRAN"
    int getFOV();
    void setFOV(int fov);
    void setName(const char* newName);
};
