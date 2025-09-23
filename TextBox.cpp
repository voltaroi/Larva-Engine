#include "TextBox.h"

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

    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos2f(x + 4, y + height / 2 - 4);
    for (char c : text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
}

void TextBox::handleKey(unsigned char key) {
    if (focused) {
        bool changed = false;

        if (key == 8 || key == 127) {
            if (!text.empty()) {
                text.pop_back();
                changed = true;
            }
        }
        else if (key >= 32 && key <= 126) {
            text += key;
            changed = true;
        }

        if (changed && onTextChanged) {
            onTextChanged(text);
        }
    }
}


bool TextBox::contains(float mx, float my) const {
    return mx >= x && mx <= x + width && my >= y && my <= y + height;
}

void TextBox::setFocus(bool f) {
    focused = f;
}
