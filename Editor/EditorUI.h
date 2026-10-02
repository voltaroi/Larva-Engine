#pragma once
#ifndef __EDITOR_UI__
#define __EDITOR_UI__
#include <string>
#include <vector>

// Interface en mode immédiat de l'éditeur. Repère écran en pixels, origine en HAUT à gauche
// (comme les coordonnées souris de GLUT), y vers le bas.
namespace ui
{
    struct Color
    {
        float r, g, b, a;
    };

    namespace col
    {
        extern const Color background, panel, panelDark, header, widget, widgetHover, widgetActive;
        extern const Color border, text, textDim, textDisabled, accent, accentHover, selection;
        extern const Color axisX, axisY, axisZ, warning, success;
    }

    enum class Font
    {
        Small,
        Normal,
        Large,
        Title
    };

    bool init(const char *fontPath);
    void beginFrame(int screenWidth, int screenHeight);
    void endFrame();
    int screenWidth();
    int screenHeight();

    // --- Entrées (à relayer depuis les callbacks GLUT) ---
    void onMouseMove(int x, int y);
    void onMouseButton(int button, bool down); // 0 gauche, 1 milieu, 2 droit
    void onWheel(int direction);
    void onChar(unsigned char c);
    void onSpecialKey(int glutKey);

    int mouseX();
    int mouseY();
    bool mouseDown(int button = 0);
    bool mousePressed(int button = 0);
    bool mouseReleased(int button = 0);
    float wheel();
    bool ctrl();
    bool shift();
    bool alt();
    // Une entrée a eu lieu pendant cette image (clic relâché, touche...) : sert à détecter les modifications
    bool hadInputEvent();
    // Caractères tapés et touches spéciales GLUT reçus depuis l'image précédente
    const std::string &typedChars();
    const std::vector<int> &specialKeys();
    // Remet l'état OpenGL du dessin 2D (après un rendu 3D)
    void setup2D();

    // Souris dans la zone et non masquée par une fenêtre superposée
    bool hover(float x, float y, float w, float h);
    // Clic gauche relâché dans la zone (appui commencé dans la zone)
    bool clicked(float x, float y, float w, float h);

    // Un champ texte a le focus clavier : l'éditeur ne doit pas traiter les raccourcis
    bool textFocused();
    void clearFocus();
    // Un widget est en cours de glissement (curseur de valeur...)
    bool widgetActive();
    // Curseur souris GLUT voulu pour cette image (appliqué par endFrame)
    void setCursor(int glutCursor);

    // --- Fenêtres superposées (menus, popups, boîtes de dialogue) ---
    // Tout ce qui est dessiné entre begin/end reçoit la souris en priorité sur le reste de l'interface
    void beginOverlay(float x, float y, float w, float h);
    void endOverlay();
    bool mouseOverOverlay();

    // --- Dessin ---
    void rect(float x, float y, float w, float h, const Color &c);
    void rectOutline(float x, float y, float w, float h, const Color &c, float thickness = 1.0f);
    void line(float x0, float y0, float x1, float y1, const Color &c, float thickness = 1.0f);
    // y = haut de la ligne de texte ; renvoie la largeur
    float text(const std::string &s, float x, float y, const Color &c, Font font = Font::Normal);
    // Texte coupé avec ".." s'il dépasse maxWidth
    void textClipped(const std::string &s, float x, float y, float maxWidth, const Color &c, Font font = Font::Normal);
    void textCentered(const std::string &s, float x, float y, float w, float h, const Color &c, Font font = Font::Normal);
    float textWidth(const std::string &s, Font font = Font::Normal);
    float lineHeight(Font font = Font::Normal);

    void pushClip(float x, float y, float w, float h);
    void popClip();

    // --- Widgets ---
    bool button(const std::string &label, float x, float y, float w, float h, bool selected = false, bool enabled = true);
    bool checkbox(const std::string &label, bool &value, float x, float y);
    // Renvoie true quand la valeur est validée (Entrée ou clic ailleurs)
    bool textField(const std::string &id, std::string &value, float x, float y, float w, float h, const char *placeholder = nullptr);
    // Glisser horizontalement pour changer la valeur, cliquer pour la saisir. Renvoie true si elle a changé.
    bool floatField(const std::string &id, float &value, float x, float y, float w, float h, float dragSpeed,
                    const Color *accent = nullptr, float minValue = -1e9f, float maxValue = 1e9f);
    // Ligne de liste cliquable
    bool listItem(const std::string &label, float x, float y, float w, float h, bool selected, const Color *dot = nullptr);
    // Zone défilante : gère la molette et la barre, renvoie le décalage à appliquer au contenu
    float scrollArea(const std::string &id, float x, float y, float w, float h, float contentHeight);

    std::string formatFloat(float v, int decimals = 3);
}

#endif
