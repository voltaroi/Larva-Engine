#pragma once
#ifndef __GAMEPAD__
#define __GAMEPAD__
#include <string>

// Manettes, volants et pédaliers (Windows) :
//   - manettes Xbox et compatibles par XInput (vibrations)
//   - tous les autres contrôleurs par DirectInput : volants de simulation, pédaliers, boîtes de vitesses,
//     manettes génériques (retour de force constant pour les volants qui le gèrent)
// Les DLL sont chargées à l'exécution : rien à lier, et sans elles le module ne voit simplement aucun périphérique.
// Ailleurs que sous Windows, toutes les fonctions sont sans effet.
namespace Gamepad
{
    const int MAX_AXES = 8;      // DirectInput : X Y Z Rx Ry Rz Curseur0 Curseur1 ; XInput : voir XAxis
    const int MAX_BUTTONS = 32;

    // Manette XInput : axes et boutons
    enum XAxis { X_LEFT_X = 0, X_LEFT_Y, X_RIGHT_X, X_RIGHT_Y, X_LEFT_TRIGGER, X_RIGHT_TRIGGER };
    enum XButton { X_A = 0, X_B, X_X, X_Y, X_LB, X_RB, X_BACK, X_START, X_LEFT_STICK, X_RIGHT_STICK, X_DPAD_UP, X_DPAD_DOWN, X_DPAD_LEFT, X_DPAD_RIGHT };

    struct DeviceInfo
    {
        std::string name;
        bool xinput = false;        // manette Xbox (axes et boutons standard)
        bool forceFeedback = false; // retour de force (volants DirectInput)
        int axes = 0, buttons = 0;
    };

    struct State
    {
        bool connected = false;
        float axis[MAX_AXES] = {};  // sticks et volant dans [-1, 1] ; gâchettes XInput dans [0, 1]
        bool button[MAX_BUTTONS] = {};
        int pov = -1;               // croix directionnelle en degrés (0 = haut), -1 = relâchée
    };

    // windowHandle : fenêtre du jeu (HWND) pour DirectInput ; nullptr = fenêtre du contexte OpenGL courant
    void init(void *windowHandle = nullptr);
    void shutdown();
    void refresh();                 // recherche les périphériques branchés (fait aussi régulièrement par update)
    void update();                  // lit tous les périphériques, une fois par image

    int count();
    const DeviceInfo &info(int device);
    const State &state(int device);

    // Vibrations d'une manette XInput (moteurs gros / petit, 0..1)
    void rumble(int device, float low, float high);
    // Retour de force d'un volant : force constante dans [-1, 1] (positif vers la droite), 0 = libre
    void setForce(int device, float force);

    const char *axisName(int device, int axis);
    std::string buttonName(int device, int button);
}

#endif
