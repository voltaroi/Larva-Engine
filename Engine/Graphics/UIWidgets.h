#pragma once
#ifndef __UI_WIDGETS__
#define __UI_WIDGETS__
#include <string>

// Widgets d'interface en mode immédiat (dessinés et testés dans le même appel, chaque image) :
// boutons, curseurs, texte centré. Coordonnées en pixels, origine en bas à gauche (comme UI).
//
// Utilisation :
//   - relayer la souris : setMouse() depuis les callbacks de mouvement, setMouseButton() pour le bouton gauche
//   - dessiner l'interface : if (UIWidgets::button("JOUER", x, y, w, h)) { ... }
//   - appeler endFrame() une fois l'interface dessinée
class UIWidgets
{
public:
    struct Theme
    {
        float button[4] = {0.12f, 0.12f, 0.16f, 0.85f};
        float buttonHover[4] = {0.9f, 0.3f, 0.1f, 0.95f};
        float buttonDisabled[4] = {0.25f, 0.25f, 0.25f, 0.6f};
        float track[4] = {0.25f, 0.25f, 0.28f, 0.9f};   // fond des curseurs
        float accent[4] = {0.9f, 0.35f, 0.1f, 1.0f};    // partie remplie des curseurs
        float radius = 10.0f;
        float textScale = 0.55f;
    };

    static Theme &theme() { return currentTheme; }

    // --- Entrées ---
    // y en pixels depuis le bas de la fenêtre
    static void setMouse(int x, int y);
    static void setMouseButton(bool down);
    static void endFrame();

    static int mouseX() { return mx; }
    static int mouseY() { return my; }
    static bool mouseDown() { return pressed; }
    static bool mouseReleased() { return released; }
    static bool hovered(float x, float y, float w, float h);
    // Relâchement du clic au-dessus de la zone pendant cette image
    static bool clicked(float x, float y, float w, float h);

    // --- Widgets ---
    static bool button(const std::string &label, float x, float y, float w, float h, bool enabled = true);
    // Bouton flèche (dir -1 gauche, +1 droite) : triangle dessiné, sans dépendre des glyphes de la police
    static bool arrowButton(int dir, float x, float y, float w, float h, bool enabled = true);
    // Valeur dans [0, 1] affichée en pourcentage ; renvoie true si elle a changé
    static bool slider(const std::string &label, float &value, float x, float y, float w, float labelWidth = 220.0f);
    // Un curseur vient d'être relâché (ex. jouer un son d'aperçu du volume)
    static bool sliderReleased();

    static void centeredText(const std::string &text, float cx, float y, float scale);

private:
    static Theme currentTheme;
    static int mx, my;
    static bool pressed, released;
    static const float *activeSlider;
};

#endif
