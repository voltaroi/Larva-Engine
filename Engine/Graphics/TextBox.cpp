#include "TextBox.h"
#include "UIWidgets.h"
#include "../Core/Clipboard.h"
#include <algorithm>
#include <cmath>

static const float TEXT_SCALE = 0.6f;
static const float PAD = 6.0f;

TextBox::TextBox(float x, float y, float width, float height)
    : x(x), y(y), width(width), height(height), focused(false), text("") {
}

static float nowSeconds() {
    return glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
}

void TextBox::sync() const {
    TextBox *self = const_cast<TextBox *>(this);
    if (text != lastText) {
        // Texte modifié de l'extérieur : curseur à la fin, plus de sélection
        self->cursor = text.size();
        anchor = (size_t)-1;
        self->allSelected = false;
        lastText = text;
    }
    if (self->cursor > text.size())
        self->cursor = text.size();
    if (anchor != (size_t)-1 && anchor > text.size())
        anchor = text.size();
}

void TextBox::touch() const {
    lastActivity = nowSeconds();
}

bool TextBox::hasSelection() const {
    return anchor != (size_t)-1 && anchor != cursor;
}

void TextBox::selection(size_t &a, size_t &b) const {
    a = std::min(anchor, cursor);
    b = std::max(anchor, cursor);
}

void TextBox::updateFlags() {
    size_t a, b;
    allSelected = false;
    if (hasSelection()) {
        selection(a, b);
        allSelected = a == 0 && b == text.size();
    }
}

void TextBox::eraseSelection() {
    if (!hasSelection())
        return;
    size_t a, b;
    selection(a, b);
    text.erase(a, b - a);
    cursor = a;
    anchor = (size_t)-1;
}

// Index du caractère dont la frontière est la plus proche de l'abscisse mx
size_t TextBox::indexAt(float mx) const {
    float rel = mx - (x + PAD) + scroll;
    size_t best = 0;
    float bestDist = 1e9f;
    for (size_t i = 0; i <= text.size(); ++i) {
        float d = std::fabs(UI::getTextWidth(text.substr(0, i), TEXT_SCALE) - rel);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void TextBox::draw() const {
    sync();
    TextBox *self = const_cast<TextBox *>(this);

    // Glisser avec le bouton enfoncé : étend la sélection
    if (dragging) {
        if (UIWidgets::mouseDown() && focused) {
            size_t i = indexAt((float)UIWidgets::mouseX());
            if (i != cursor) {
                self->cursor = i;
                self->updateFlags();
                touch();
            }
        } else {
            dragging = false;
            if (anchor == cursor)
                anchor = (size_t)-1;
        }
    }

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

    // Le texte défile pour garder le curseur visible
    float visible = width - 2.0f * PAD;
    float caretW = UI::getTextWidth(text.substr(0, cursor), TEXT_SCALE);
    float totalW = UI::getTextWidth(text, TEXT_SCALE);
    if (totalW + 2.0f <= visible)
        scroll = 0.0f;
    else {
        if (caretW - scroll > visible)
            scroll = caretW - visible;
        if (caretW - scroll < 0.0f)
            scroll = caretW;
        scroll = std::max(0.0f, std::min(scroll, totalW + 2.0f - visible));
    }

    // Découpe : le texte ne déborde pas du champ
    GLboolean wasScissor = glIsEnabled(GL_SCISSOR_TEST);
    GLint oldBox[4];
    glGetIntegerv(GL_SCISSOR_BOX, oldBox);
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    GLint sx = std::max((GLint)vp[0], (GLint)(x + 2.0f)), sy = (GLint)y;
    GLint ex = (GLint)(x + width - 2.0f), ey = (GLint)(y + height);
    if (wasScissor) {
        sx = std::max(sx, oldBox[0]);
        sy = std::max(sy, oldBox[1]);
        ex = std::min(ex, oldBox[0] + oldBox[2]);
        ey = std::min(ey, oldBox[1] + oldBox[3]);
    }
    glEnable(GL_SCISSOR_TEST);
    glScissor(sx, sy, std::max(0, ex - sx), std::max(0, ey - sy));

    float textX = x + PAD - scroll;
    if (focused && hasSelection()) {
        // Texte sélectionné : surligné
        size_t a, b;
        selection(a, b);
        float x0 = textX + UI::getTextWidth(text.substr(0, a), TEXT_SCALE);
        float x1 = textX + UI::getTextWidth(text.substr(0, b), TEXT_SCALE);
        glColor3f(0.55f, 0.75f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(x0, y + 5.0f);
        glVertex2f(x1 + 2.0f, y + 5.0f);
        glVertex2f(x1 + 2.0f, y + height - 5.0f);
        glVertex2f(x0, y + height - 5.0f);
        glEnd();
    }

    UI::setColor(0.0f, 0.0f, 0.0f, 1.0f);
    float textY = y + (height / 2.0f) - 8.0f;
    UI::renderText(text, textX, textY, TEXT_SCALE);

    // Barre du curseur : clignote, mais reste allumée juste après une action
    if (focused) {
        float t = nowSeconds() - lastActivity;
        bool on = t < 0.5f || std::fmod(t, 1.0f) < 0.55f;
        if (on) {
            float cx = textX + caretW;
            glColor3f(0.0f, 0.0f, 0.0f);
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            glVertex2f(cx, y + 6.0f);
            glVertex2f(cx, y + height - 6.0f);
            glEnd();
            glLineWidth(1.0f);
        }
    }

    if (wasScissor)
        glScissor(oldBox[0], oldBox[1], oldBox[2], oldBox[3]);
    else
        glDisable(GL_SCISSOR_TEST);
}

void TextBox::insert(const std::string &s) {
    sync();
    eraseSelection();
    for (char c : s) {
        if (maxLength > 0 && text.size() >= maxLength)
            break;
        if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
            text.insert(text.begin() + cursor, c);
            ++cursor;
        }
    }
    lastText = text;
    allSelected = false;
}

void TextBox::handleKey(unsigned char key) {
    if (!focused)
        return;
    sync();
    std::string before = text;

    if (key == 1) {             // Ctrl+A
        anchor = 0;
        cursor = text.size();
        updateFlags();
        touch();
        return;
    }
    if (key == 3 || key == 24) { // Ctrl+C, Ctrl+X : la sélection, ou tout le champ s'il n'y en a pas
        size_t a = 0, b = text.size();
        if (hasSelection())
            selection(a, b);
        Clipboard::setText(text.substr(a, b - a));
        if (key == 24) {
            if (hasSelection())
                eraseSelection();
            else {
                text.clear();
                cursor = 0;
            }
        }
    }
    else if (key == 22) {       // Ctrl+V : sans espaces ni retours à la ligne autour (adresse copiée d'un chat...)
        std::string pasted = Clipboard::getText();
        size_t a = pasted.find_first_not_of(" \t\r\n");
        size_t b = pasted.find_last_not_of(" \t\r\n");
        pasted = a == std::string::npos ? "" : pasted.substr(a, b - a + 1);
        insert(pasted);
    }
    else if (key == 8) {        // Retour arrière
        if (hasSelection())
            eraseSelection();
        else if (cursor > 0) {
            text.erase(cursor - 1, 1);
            --cursor;
        }
    }
    else if (key == 127) {      // Suppr
        if (hasSelection())
            eraseSelection();
        else if (cursor < text.size())
            text.erase(cursor, 1);
    }
    else if (key >= 32 && key <= 126) {
        insert(std::string(1, (char)key));
    }
    else {
        return;
    }
    anchor = (size_t)-1;
    allSelected = false;
    lastText = text;
    touch();

    if (text != before && onTextChanged)
        onTextChanged(text);
}

bool TextBox::handleSpecial(int key, int mods) {
    if (!focused)
        return false;
    sync();
    const bool shift = (mods & GLUT_ACTIVE_SHIFT) != 0, ctrl = (mods & GLUT_ACTIVE_CTRL) != 0;
    size_t target = cursor;
    switch (key) {
    case GLUT_KEY_LEFT:
        if (!shift && hasSelection()) {
            size_t a, b;
            selection(a, b);
            target = a;
        }
        else if (ctrl) {
            while (target > 0 && text[target - 1] == ' ')
                --target;
            while (target > 0 && text[target - 1] != ' ')
                --target;
        }
        else if (target > 0)
            --target;
        break;
    case GLUT_KEY_RIGHT:
        if (!shift && hasSelection()) {
            size_t a, b;
            selection(a, b);
            target = b;
        }
        else if (ctrl) {
            while (target < text.size() && text[target] != ' ')
                ++target;
            while (target < text.size() && text[target] == ' ')
                ++target;
        }
        else if (target < text.size())
            ++target;
        break;
    case GLUT_KEY_HOME:
        target = 0;
        break;
    case GLUT_KEY_END:
        target = text.size();
        break;
    default:
        return false;
    }
    if (shift) {
        if (anchor == (size_t)-1)
            anchor = cursor;
    } else {
        anchor = (size_t)-1;
    }
    cursor = target;
    updateFlags();
    touch();
    return true;
}

bool TextBox::contains(float mx, float my) const {
    return mx >= x && mx <= x + width && my >= y && my <= y + height;
}

void TextBox::click(float mx, float my) {
    if (!contains(mx, my)) {
        setFocus(false);
        return;
    }
    focused = true;
    sync();
    cursor = indexAt(mx);
    anchor = cursor;
    dragging = true;
    allSelected = false;
    touch();
}

void TextBox::setFocus(bool f) {
    if (f && !focused) {
        sync();
        cursor = text.size();
        touch();
    }
    focused = f;
    if (!f) {
        anchor = (size_t)-1;
        allSelected = false;
        dragging = false;
    }
}
