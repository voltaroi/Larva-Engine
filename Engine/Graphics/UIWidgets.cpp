#include "UIWidgets.h"
#include "UI.h"
#include "Draw2D.h"
#include <algorithm>
#include <cmath>

UIWidgets::Theme UIWidgets::currentTheme;
int UIWidgets::mx = 0;
int UIWidgets::my = 0;
bool UIWidgets::pressed = false;
bool UIWidgets::released = false;
const float *UIWidgets::activeSlider = nullptr;

void UIWidgets::setMouse(int x, int y)
{
    mx = x;
    my = y;
}

void UIWidgets::setMouseButton(bool down)
{
    if (pressed && !down)
        released = true;
    pressed = down;
}

void UIWidgets::endFrame()
{
    if (!pressed)
    {
        activeSlider = nullptr;
    }
    released = false;
}

bool UIWidgets::hovered(float x, float y, float w, float h)
{
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

bool UIWidgets::clicked(float x, float y, float w, float h)
{
    return released && hovered(x, y, w, h);
}

bool UIWidgets::button(const std::string &label, float x, float y, float w, float h, bool enabled)
{
    const Theme &t = currentTheme;
    bool hover = enabled && hovered(x, y, w, h);
    const float *c = !enabled ? t.buttonDisabled : (hover ? t.buttonHover : t.button);
    UI::drawBox(x, y, w, h, c[0], c[1], c[2], c[3], false, t.radius);
    const UI::Style &st = UI::style();
    if (st.buttonBar > 0.0f)
    {
        // Barre d'accent collée sous le bouton, dans le prolongement de son bord penché
        float shift = std::min(st.maxShift, std::fabs(st.slant) * h);
        if (w < shift * 2.0f || (st.uprightAbove > 0.0f && h >= st.uprightAbove))
            shift = 0.0f;
        const float lean = st.slant < 0.0f ? -1.0f : 1.0f;
        const float bx = x - lean * (shift * 0.5f + std::fabs(st.slant) * st.buttonBar * 0.5f);
        UI::drawBox(bx, y - st.buttonBar, w, st.buttonBar, t.accent[0], t.accent[1], t.accent[2], enabled ? t.accent[3] : t.accent[3] * 0.35f);
    }

    // Texte réduit s'il déborde du bouton
    float scale = t.textScale;
    float tw = UI::getTextWidth(label, scale);
    if (tw > w - 24.0f)
    {
        scale *= (w - 24.0f) / tw;
        tw = UI::getTextWidth(label, scale);
    }
    UI::setColor(1.0f, 1.0f, 1.0f, enabled ? 1.0f : 0.5f);
    UI::renderText(label, x + (w - tw) / 2.0f, y + h / 2.0f - 8.0f, scale);
    UI::setColor(1.0f, 1.0f, 1.0f, 1.0f);
    return hover && released;
}

bool UIWidgets::arrowButton(int dir, float x, float y, float w, float h, bool enabled)
{
    bool hit = button("", x, y, w, h, enabled);
    Draw2D::setColor(1.0f, 1.0f, 1.0f, enabled ? 1.0f : 0.35f);
    Draw2D::arrow(x + w * 0.5f, y + h * 0.5f, std::min(w, h) * 0.22f, (float)dir, 0.0f);
    return hit;
}

bool UIWidgets::slider(const std::string &label, float &value, float x, float y, float w, float labelWidth)
{
    const Theme &t = currentTheme;
    UI::renderText(label, x, y + 2, 0.45f);
    float bx = x + labelWidth, bw = w - labelWidth - 80, by = y + 4, bh = 14;
    bool hover = mx >= bx - 12 && mx <= bx + bw + 12 && my >= y - 8 && my <= y + 30;
    bool changed = false;
    if (pressed && (activeSlider == &value || (activeSlider == nullptr && hover)))
    {
        activeSlider = &value;
        float v = std::max(0.0f, std::min(1.0f, (mx - bx) / bw));
        changed = v != value;
        value = v;
    }
    UI::drawBox(bx, by, bw, bh, t.track[0], t.track[1], t.track[2], t.track[3], false, 7.0f);
    if (value > 0.0f)
        UI::drawBox(bx, by, std::max(14.0f, bw * value), bh, t.accent[0], t.accent[1], t.accent[2], t.accent[3], false, 7.0f);
    bool active = activeSlider == &value || hover;
    float knob = active ? 1.0f : 0.85f;
    UI::drawBox(bx + bw * value - 10, by - 6, 20, 26, knob, knob, active ? 1.0f : 0.88f, 1.0f, false, 9.0f);
    UI::renderText(std::to_string((int)std::round(value * 100.0f)) + "%", bx + bw + 20, y + 2, 0.42f);
    return changed;
}

bool UIWidgets::sliderReleased()
{
    return !pressed && released && activeSlider != nullptr;
}

void UIWidgets::centeredText(const std::string &text, float cx, float y, float scale)
{
    UI::renderText(text, cx - UI::getTextWidth(text, scale) / 2.0f, y, scale);
}
