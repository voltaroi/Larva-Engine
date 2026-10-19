#include "UIWidgets.h"
#include "UI.h"
#include "Draw2D.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <unordered_map>

UIWidgets::Theme UIWidgets::currentTheme;
bool UIWidgets::bloodHover = false;
bool UIWidgets::hoverNew = false;
bool UIWidgets::hoverSeen = false;
unsigned long long UIWidgets::hoverKey = 0;
int UIWidgets::mx = 0;
int UIWidgets::my = 0;
bool UIWidgets::pressed = false;
bool UIWidgets::released = false;
const float *UIWidgets::activeSlider = nullptr;
bool UIWidgets::modalWanted = false;
bool UIWidgets::modalActive = false;
bool UIWidgets::inModal = false;

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
    if (!hoverSeen)
        hoverKey = 0;
    hoverSeen = hoverNew = false;
    modalActive = modalWanted;
    modalWanted = inModal = false;
}

void UIWidgets::beginModal()
{
    modalWanted = inModal = true;
}

bool UIWidgets::hovered(float x, float y, float w, float h)
{
    if (blocked())
        return false;
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

bool UIWidgets::clicked(float x, float y, float w, float h)
{
    return released && hovered(x, y, w, h);
}

// Survol « sang » : le rouge coule du haut du bouton en une nappe au bord ondulé, d'où pendent quelques coulures arrondies ;
// il ne remplit pas le bouton et n'en sort jamais. Quand la souris s'en va, le sang remonte comme il est venu.
// Chaque bouton est reconnu par sa place à l'écran.
void UIWidgets::drawBlood(float x, float y, float w, float h, bool hover)
{
    struct Anim
    {
        float level; // 0 : rien, 1 : sang entièrement coulé
        double last;
    };
    static std::unordered_map<unsigned long long, Anim> anims;
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const unsigned long long key = ((unsigned long long)(unsigned)(int)x << 40) ^ ((unsigned long long)(unsigned)(int)y << 20) ^ ((unsigned long long)(unsigned)(int)w << 8) ^ (unsigned)(int)h;
    auto it = anims.find(key);
    if (it == anims.end())
    {
        if (!hover)
            return;
        it = anims.emplace(key, Anim{0.0f, now}).first;
    }
    Anim &an = it->second;
    const float dt = (float)(now - an.last);
    an.last = now;
    if (dt > 0.3f) // le bouton n'était plus affiché : on repart de zéro
        an.level = 0.0f;
    else
        an.level = std::max(0.0f, std::min(1.0f, an.level + (hover ? dt * 1.9f : -dt * 2.6f)));
    const float level = an.level;
    if (level <= 0.0f)
    {
        if (!hover)
            anims.erase(it);
        return;
    }
    auto rnd = [key](int i, int salt) {
        unsigned v = (unsigned)(key ^ (key >> 32)) + (unsigned)i * 2654435761u + (unsigned)salt * 40503u;
        v ^= v >> 13;
        v *= 1274126177u;
        return ((v >> 8) & 0xFFFF) / 65535.0f;
    };
    auto ease = [](float k) { k = std::max(0.0f, std::min(1.0f, k)); return k * k * (3.0f - 2.0f * k); };
    // Bruit lisse le long du bouton (valeurs tirées tous les `step` pixels, raccordées en douceur)
    auto wave = [&](float u, float step, int salt) {
        const float f = u / step;
        const int c = (int)std::floor(f);
        const float k = ease(f - c);
        return rnd(c, salt) * (1.0f - k) + rnd(c + 1, salt) * k;
    };
    const float cw = 2.0f, cell = 46.0f;
    for (float u = 0.0f; u < w; u += cw)
    {
        // La nappe : elle descend plus vite par endroits, et s'arrête sur un bord ondulé vers le tiers du bouton
        const float k = ease(level * (1.5f + 1.3f * wave(u, 60.0f, 1)));
        float depth = h * (0.26f + 0.14f * wave(u, 34.0f, 2) + 0.05f * wave(u, 11.0f, 3)) * k;
        // Les coulures : une par tranche de bouton environ, au bout rond, qui s'allongent après la nappe
        const int c0 = (int)std::floor(u / cell);
        for (int c = c0 - 1; c <= c0 + 1; ++c)
        {
            if (rnd(c, 4) < 0.25f)
                continue;
            const float center = (c + 0.25f + 0.5f * rnd(c, 5)) * cell, half = 3.5f + 3.5f * rnd(c, 6), d = std::fabs(u + cw * 0.5f - center) / half;
            if (d >= 1.0f)
                continue;
            const float grow = ease((level - 0.25f - 0.2f * rnd(c, 7)) / 0.55f);
            depth += h * (0.2f + 0.3f * rnd(c, 8)) * grow * std::sqrt(1.0f - d * d) * (0.55f + 0.45f * (1.0f - d * d));
        }
        // Bords du bouton : la nappe s'arrondit au lieu de finir à angle droit
        const float edge = std::min(u + cw * 0.5f, w - u - cw * 0.5f);
        if (edge < 8.0f)
            depth *= std::sqrt(std::max(0.0f, 1.0f - (1.0f - edge / 8.0f) * (1.0f - edge / 8.0f)));
        depth = std::min(depth, h * 0.92f);
        if (depth < 0.5f)
            continue;
        const float bw = std::min(cw, w - u);
        UI::drawBox(x + u, y + h - depth, bw, depth, 0.56f, 0.02f, 0.04f, 0.97f);
        // Liseré plus sombre au bord du sang : il lui donne de l'épaisseur
        UI::drawBox(x + u, y + h - depth, bw, std::min(depth, 2.0f), 0.3f, 0.0f, 0.02f, 0.9f);
    }
}

bool UIWidgets::button(const std::string &label, float x, float y, float w, float h, bool enabled)
{
    const Theme &t = currentTheme;
    bool hover = enabled && hovered(x, y, w, h);
    if (hover)
    {
        // Bouton reconnu par sa place à l'écran : on note quand la souris vient d'y entrer
        const unsigned long long key = (((unsigned long long)(unsigned)(int)x << 40) ^ ((unsigned long long)(unsigned)(int)y << 20) ^ ((unsigned long long)(unsigned)(int)w << 8) ^ (unsigned)(int)h) | 1ull;
        if (key != hoverKey)
            hoverNew = true;
        hoverKey = key;
        hoverSeen = true;
    }
    const float *c = !enabled ? t.buttonDisabled : (hover && !bloodHover ? t.buttonHover : t.button);
    UI::drawBox(x, y, w, h, c[0], c[1], c[2], c[3], false, t.radius);
    if (bloodHover && enabled)
        drawBlood(x, y, w, h, hover);
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

bool UIWidgets::slider(const std::string &label, float &value, float x, float y, float w, float labelWidth, const std::string &valueText)
{
    const Theme &t = currentTheme;
    UI::renderText(label, x, y + 2, 0.45f);
    float bx = x + labelWidth, bw = w - labelWidth - 80, by = y + 4, bh = 14;
    bool hover = !blocked() && mx >= bx - 12 && mx <= bx + bw + 12 && my >= y - 8 && my <= y + 30;
    bool changed = false;
    if (pressed && !blocked() && (activeSlider == &value || (activeSlider == nullptr && hover)))
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
    UI::renderText(valueText.empty() ? std::to_string((int)std::round(value * 100.0f)) + "%" : valueText, bx + bw + 20, y + 2, 0.42f);
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
