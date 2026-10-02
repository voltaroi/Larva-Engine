#pragma once
#include <string>
#include <GL/glut.h>
#include <functional>
#include "UI.h"

class TextBox {
public:
    float x, y, width, height;
    bool focused;
    std::string text;
    size_t maxLength = 0;     // 0 : pas de limite
    bool allSelected = false; // Ctrl+A : la prochaine frappe remplace tout le texte

    std::function<void(const std::string&)> onTextChanged;

    TextBox(float x, float y, float width, float height);

    void draw() const;
    // Caractères tapés, plus Retour arrière, Ctrl+A (tout sélectionner), Ctrl+C, Ctrl+X et Ctrl+V (presse-papiers)
    void handleKey(unsigned char key);
    // Ajoute du texte (caractères imprimables uniquement, coupé à maxLength)
    void insert(const std::string &s);
    bool contains(float mx, float my) const;
    void setFocus(bool f);
};
