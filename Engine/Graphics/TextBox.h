#pragma once
#include <string>
#include <GL/glut.h>
#include <functional>
#include "UI.h"

// Champ de texte sur une ligne : curseur (barre clignotante), sélection (Ctrl+A, Maj+flèches, glisser à la souris),
// clic pour placer le curseur, texte qui défile quand il dépasse du champ.
// Le texte reste public : si le code le change directement (text = ..., text.clear()), le curseur passe à la fin.
class TextBox {
public:
    float x, y, width, height;
    bool focused;
    std::string text;
    size_t maxLength = 0;     // 0 : pas de limite
    bool allSelected = false; // vrai quand tout le texte est sélectionné (Ctrl+A) : la prochaine frappe le remplace
    size_t cursor = 0;        // position du curseur dans le texte (0 .. text.size())

    std::function<void(const std::string&)> onTextChanged;

    TextBox(float x, float y, float width, float height);

    void draw() const;
    // Caractères tapés, plus Retour arrière, Suppr, Ctrl+A (tout sélectionner), Ctrl+C, Ctrl+X et Ctrl+V (presse-papiers)
    void handleKey(unsigned char key);
    // Touches spéciales GLUT (GLUT_KEY_LEFT, RIGHT, HOME, END) ; mods = glutGetModifiers() (Maj sélectionne, Ctrl saute un mot)
    // Renvoie true si la touche a été utilisée
    bool handleSpecial(int key, int mods);
    // Insère du texte au curseur (remplace la sélection ; caractères imprimables uniquement, coupé à maxLength)
    void insert(const std::string &s);
    bool contains(float mx, float my) const;
    // Clic gauche : donne le focus et place le curseur sous la souris si le clic est dans le champ, sinon retire le focus.
    // Garder le bouton enfoncé et glisser sélectionne (suivi par draw via UIWidgets)
    void click(float mx, float my);
    void setFocus(bool f);

private:
    mutable std::string lastText;       // pour détecter un changement de text fait de l'extérieur
    mutable size_t anchor = (size_t)-1; // début de la sélection (npos : aucune)
    mutable float scroll = 0.0f;        // décalage horizontal du texte
    mutable bool dragging = false;
    mutable float lastActivity = 0.0f;  // dernière frappe/clic, pour le clignotement
    void sync() const;
    size_t indexAt(float mx) const;
    bool hasSelection() const;
    void selection(size_t &a, size_t &b) const;
    void eraseSelection();
    void touch() const;
    void updateFlags();
};
