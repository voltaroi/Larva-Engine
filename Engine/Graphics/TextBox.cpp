#include "TextBox.h"
#include "../Core/Clipboard.h"

TextBox::TextBox(float x, float y, float width, float height)
    : x(x), y(y), width(width), height(height), focused(false), text("") {
}

void TextBox::draw() const {
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();

    glColor3f(focused ? 0.0f : 0.3f, 0.3f, 0.3f);
    glLineWidth(focused ? 2.0f : 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();

    if (focused && allSelected && !text.empty()) {
        // Texte sélectionné : surligné
        float w = UI::getTextWidth(text, 0.6f);
        glColor3f(0.55f, 0.75f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(x + 4.0f, y + 5.0f);
        glVertex2f(x + 8.0f + w, y + 5.0f);
        glVertex2f(x + 8.0f + w, y + height - 5.0f);
        glVertex2f(x + 4.0f, y + height - 5.0f);
        glEnd();
    }

    UI::setColor(0.0f, 0.0f, 0.0f, 1.0f);
    float textX = x + 6.0f;
    float textY = y + (height / 2.0f) - 8.0f;
    UI::renderText(text, textX, textY, 0.6f);
}

void TextBox::insert(const std::string &s) {
    for (char c : s) {
        if (maxLength > 0 && text.size() >= maxLength)
            break;
        if ((unsigned char)c >= 32 && (unsigned char)c <= 126)
            text += c;
    }
}

void TextBox::handleKey(unsigned char key) {
    if (!focused)
        return;
    std::string before = text;

    if (key == 1) {             // Ctrl+A
        allSelected = true;
        return;
    }
    if (key == 3 || key == 24) { // Ctrl+C, Ctrl+X : tout le champ (ou la sélection, qui est tout le champ)
        Clipboard::setText(text);
        if (key == 24)
            text.clear();
    }
    else if (key == 22) {       // Ctrl+V : sans espaces ni retours à la ligne autour (adresse copiée d'un chat...)
        std::string pasted = Clipboard::getText();
        size_t a = pasted.find_first_not_of(" \t\r\n");
        size_t b = pasted.find_last_not_of(" \t\r\n");
        pasted = a == std::string::npos ? "" : pasted.substr(a, b - a + 1);
        if (allSelected)
            text.clear();
        insert(pasted);
    }
    else if (key == 8 || key == 127) {
        if (allSelected)
            text.clear();
        else if (!text.empty())
            text.pop_back();
    }
    else if (key >= 32 && key <= 126) {
        if (allSelected)
            text.clear();
        insert(std::string(1, (char)key));
    }
    else {
        return;
    }
    allSelected = false;

    if (text != before && onTextChanged)
        onTextChanged(text);
}

bool TextBox::contains(float mx, float my) const {
    return mx >= x && mx <= x + width && my >= y && my <= y + height;
}

void TextBox::setFocus(bool f) {
    focused = f;
    if (!f)
        allSelected = false;
}
