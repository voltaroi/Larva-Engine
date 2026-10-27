#include "UIWidgets.h"
#include "UI.h"
#include "Draw2D.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <unordered_map>

UIWidgets::Theme UIWidgets::currentTheme;
bool UIWidgets::bloodHover = false;
bool UIWidgets::blinkHover = false;
double UIWidgets::hoverStart = 0.0;
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

// Fond abîmé d'un grand bouton, tracé en tranches : la forme (bords en dents, coins arrachés) et les taches sont tirées de la
// place du bouton à l'écran, donc chaque bouton a la sienne et elle ne bouge pas d'une image à l'autre.
void UIWidgets::drawPlate(int style, float x, float y, float w, float h, bool hover, bool enabled, float shade)
{
    const unsigned long long key = ((unsigned long long)(unsigned)(int)x << 40) ^ ((unsigned long long)(unsigned)(int)y << 20) ^ ((unsigned long long)(unsigned)(int)w << 8) ^ (unsigned)(int)h;
    auto rnd = [key](int i, int salt) {
        unsigned v = (unsigned)(key ^ (key >> 32)) + (unsigned)i * 2654435761u + (unsigned)salt * 40503u;
        v ^= v >> 13;
        v *= 1274126177u;
        v ^= v >> 15;
        return ((v >> 8) & 0xFFFF) / 65535.0f;
    };
    const float dim = (enabled ? 1.0f : 0.55f) * shade;
    auto box = [&](float bx, float by, float bw, float bh, float r, float g, float b, float a, float rad = 0.0f) {
        if (bw > 0.0f && bh > 0.0f)
            UI::drawBox(bx, by, bw, bh, r * dim, g * dim, b * dim, a, false, rad);
    };
    // Taches rondes dans la zone [x0, x1] x [y0, y1]
    auto blotches = [&](int n, int salt, float x0, float x1, float y0, float y1, float r, float g, float b, float a, float minW, float maxW) {
        for (int i = 0; i < n; ++i)
        {
            const float bw = minW + (maxW - minW) * rnd(i, salt), bh = std::min(y1 - y0, bw * (0.3f + 0.3f * rnd(i, salt + 1)) + 3.0f);
            const float bx = x0 + (x1 - x0 - bw) * rnd(i, salt + 2), by = y0 + (y1 - y0 - bh) * rnd(i, salt + 3);
            box(bx, by, bw, bh, r, g, b, a * (0.5f + 0.5f * rnd(i, salt + 4)), std::min(bw, bh) * 0.5f);
        }
    };
    auto scratches = [&](int n, int salt, float x0, float x1, float y0, float y1, float r, float g, float b, float a) {
        Draw2D::setLineWidth(1.0f);
        Draw2D::setColor(r * dim, g * dim, b * dim, a);
        for (int i = 0; i < n; ++i)
        {
            const float sx = x0 + (x1 - x0) * rnd(i, salt), sy = y0 + (y1 - y0) * rnd(i, salt + 1), len = 18.0f + 60.0f * rnd(i, salt + 2), ang = (rnd(i, salt + 3) - 0.5f) * 0.6f;
            Draw2D::line(sx, sy, std::min(x1, sx + len * std::cos(ang)), std::max(y0, std::min(y1, sy + len * std::sin(ang))));
        }
    };
    auto rivet = [&](float cx, float cy) {
        box(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, 0.09f, 0.075f, 0.07f, 1.0f, 5.0f);
        box(cx - 4.0f, cy - 3.0f, 8.0f, 8.0f, 0.3f, 0.27f, 0.25f, 1.0f, 4.0f);
        box(cx - 2.5f, cy, 3.0f, 3.0f, 0.62f, 0.58f, 0.54f, 0.9f, 1.5f);
    };

    if (style == 1)
    {
        // Plaque de métal rouillée : bords en dents, taches de rouille et de crasse, rayures, quatre rivets
        const float amp = 6.0f, col = 6.0f;
        const float cH[3] = {0.36f, 0.09f, 0.08f}, cN[3] = {0.19f, 0.155f, 0.145f};
        const float *c = hover ? cH : cN;
        box(x, y + amp, w, h - 2.0f * amp, c[0], c[1], c[2], 1.0f);
        for (float u = 0.0f; u < w; u += col)
        {
            const int i = (int)(u / col);
            const float bw = std::min(col, w - u), ti = amp * rnd(i, 1), bi = amp * rnd(i, 2);
            box(x + u, y + h - amp, bw, amp - ti, c[0], c[1], c[2], 1.0f);
            box(x + u, y + bi, bw, amp - bi, c[0], c[1], c[2], 1.0f);
            // liseré sombre au bord de la tôle
            box(x + u, y + h - ti - 2.0f, bw, 2.0f, 0.05f, 0.04f, 0.04f, 1.0f);
            box(x + u, y + bi, bw, 2.0f, 0.05f, 0.04f, 0.04f, 1.0f);
        }
        blotches(34, 10, x + 3.0f, x + w - 3.0f, y + amp, y + h - amp, 0.47f, 0.21f, 0.08f, 0.38f, 12.0f, 46.0f);
        blotches(22, 40, x + 3.0f, x + w - 3.0f, y + amp, y + h - amp, 0.06f, 0.045f, 0.04f, 0.5f, 10.0f, 38.0f);
        scratches(14, 70, x + 8.0f, x + w - 8.0f, y + amp + 2.0f, y + h - amp - 2.0f, 0.65f, 0.57f, 0.52f, 0.3f);
        rivet(x + 14.0f, y + h - 15.0f);
        rivet(x + w - 14.0f, y + h - 15.0f);
        rivet(x + 14.0f, y + 15.0f);
        rivet(x + w - 14.0f, y + 15.0f);
        // coulures de rouille sous les rivets du haut
        box(x + 12.0f, y + h - 34.0f, 3.0f, 17.0f, 0.4f, 0.17f, 0.06f, 0.6f, 1.5f);
        box(x + w - 16.0f, y + h - 28.0f, 3.0f, 11.0f, 0.4f, 0.17f, 0.06f, 0.6f, 1.5f);
    }
    else if (style == 2)
    {
        // Planche clouée : extrémités déchirées en dents de scie, veines du bois, taches de sang séché, deux clous
        const int rows = 9;
        const float rh = h / rows;
        const float cH[3] = {0.38f, 0.11f, 0.085f}, cN[3] = {0.23f, 0.2f, 0.16f};
        const float *c = hover ? cH : cN;
        for (int r = 0; r < rows; ++r)
        {
            const float li = 16.0f * rnd(r, 11) + ((r & 1) ? 0.0f : 5.0f), ri = 16.0f * rnd(r, 12) + ((r & 1) ? 5.0f : 0.0f);
            const float shade = 0.9f + 0.2f * rnd(r, 13);
            box(x + li, y + r * rh, w - li - ri, rh + 0.5f, c[0] * shade, c[1] * shade, c[2] * shade, 1.0f);
        }
        // veines : traits sombres un peu ondulés sur toute la longueur
        for (int v = 0; v < 15; ++v)
        {
            const float vy = y + 4.0f + (h - 8.0f) * rnd(v, 20);
            float sx = x + 14.0f + 40.0f * rnd(v, 21);
            Draw2D::setLineWidth(1.0f);
            Draw2D::setColor(0.09f * dim, 0.07f * dim, 0.05f * dim, 0.5f);
            for (int s = 0; s < 14 && sx < x + w - 22.0f; ++s)
            {
                const float len = 14.0f + 26.0f * rnd(v * 20 + s, 22), dy = (rnd(v * 20 + s, 23) - 0.5f) * 3.0f;
                Draw2D::line(sx, vy, std::min(x + w - 18.0f, sx + len), vy + dy);
                sx += len + 4.0f + 14.0f * rnd(v * 20 + s, 24);
            }
        }
        blotches(18, 30, x + 18.0f, x + w - 18.0f, y + 3.0f, y + h - 3.0f, 0.4f, 0.06f, 0.05f, 0.4f, 14.0f, 52.0f);
        blotches(18, 60, x + 18.0f, x + w - 18.0f, y + 3.0f, y + h - 3.0f, 0.05f, 0.035f, 0.025f, 0.45f, 10.0f, 40.0f);
        box(x, y, w, 2.0f, 0.05f, 0.04f, 0.03f, 0.7f);
        box(x, y + h - 2.0f, w, 2.0f, 0.05f, 0.04f, 0.03f, 0.7f);
        for (const float nx : {x + 26.0f, x + w - 26.0f})
        {
            box(nx - 5.0f, y + h * 0.5f - 5.0f, 10.0f, 10.0f, 0.1f, 0.095f, 0.095f, 1.0f, 5.0f);
            box(nx - 3.0f, y + h * 0.5f - 2.0f, 6.0f, 6.0f, 0.42f, 0.42f, 0.42f, 1.0f, 3.0f);
            box(nx - 1.5f, y + h * 0.5f - 22.0f, 3.0f, 17.0f, 0.3f, 0.12f, 0.06f, 0.55f, 1.5f);
        }
    }
    else if (style >= 9)
    {
        // Touche d'après la maquette : face beige mate et lisse, fin reflet en haut et à gauche, et en bas une bande sombre
        // (l'épaisseur de la touche vue de dessus). Presque pas de contour, un simple grain. 9 : clair, 10 : plus sombre.
        // Survolée (sans sang) : la touche s'enfonce (bande réduite) et rougit.
        const float k = style == 9 ? 1.0f : 0.68f;
        const float bb = hover ? 1.5f : std::min(4.0f, 1.5f + h * 0.05f), e = 1.5f, re = 2.0f;
        const float fr = hover ? 0.62f : 0.585f, fg = hover ? 0.43f : 0.575f, fb = hover ? 0.38f : 0.5f;
        box(x, y, w, h, 0.05f, 0.045f, 0.04f, 1.0f, 3.0f);                                           // ombre portée très fine
        box(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 0.3f * k, 0.28f * k, 0.23f * k, 1.0f, 2.0f);       // flancs de la touche
        box(x + 1.0f, y + 1.0f + bb, w - 2.0f - re, h - 2.0f - bb, fr * k, fg * k, fb * k, 1.0f, 2.0f); // face
        box(x + 1.0f, y + h - 1.0f - e, w - 2.0f - re, e, 0.7f * k, 0.69f * k, 0.62f * k, 1.0f);        // reflet haut
        box(x + 1.0f, y + 1.0f + bb, e, h - 2.0f - bb - e, 0.68f * k, 0.67f * k, 0.6f * k, 1.0f);      // reflet gauche
        box(x + w - 1.0f - re - 1.0f, y + 1.0f + bb, 1.0f, h - 2.0f - bb, 0.0f, 0.0f, 0.0f, 0.18f);    // arête droite de la face
        box(x + 1.0f + e, y + 1.0f + bb, w - 2.0f - re - e, std::max(2.0f, (h - bb) * 0.28f), 0.0f, 0.0f, 0.0f, 0.06f); // face un peu plus sombre en bas
        const float fx0 = x + 2.0f + e, fx1 = x + w - 1.0f - re, fy0 = y + 1.0f + bb, fy1 = y + h - 1.0f - e;
        for (int i = 0; i < (int)(w * h / 90.0f); ++i) // grain fin
        {
            const bool lt = rnd(i, 40) > 0.5f;
            box(fx0 + (fx1 - fx0 - 2.0f) * rnd(i, 41), fy0 + (fy1 - fy0 - 1.0f) * rnd(i, 42), 1.0f + (rnd(i, 43) > 0.7f ? 1.0f : 0.0f), 1.0f,
                lt ? 0.85f : 0.2f, lt ? 0.84f : 0.19f, lt ? 0.76f : 0.16f, 0.07f);
        }
    }
    else if (style == 7 || style == 8)
    {
        const float k = style == 8 ? 0.36f : 1.0f; // 8 : même touche, bien plus sombre
        // Touche de vieil appareil : plastique beige, biseau clair en haut à gauche et sombre en bas à droite, usure et crasse.
        // Survolée (sans sang), elle paraît enfoncée : les biseaux s'inversent et le plastique rougit.
        const float cH[3] = {0.5f * k + 0.1f * (1 - k), 0.36f * k, 0.33f * k}, cN[3] = {0.6f * k, 0.58f * k, 0.52f * k};
        const float *c = hover ? cH : cN;
        const float lr = (hover ? 0.3f : 0.82f) * (0.4f + 0.6f * k), lg = (hover ? 0.27f : 0.8f) * (0.4f + 0.6f * k), lb = (hover ? 0.24f : 0.74f) * (0.4f + 0.6f * k);
        const float dr = (hover ? 0.82f : 0.3f) * (0.4f + 0.6f * k) * (hover ? 1.0f : k + 0.2f), dg = (hover ? 0.8f : 0.28f) * (0.4f + 0.6f * k) * (hover ? 1.0f : k + 0.2f), db = (hover ? 0.74f : 0.25f) * (0.4f + 0.6f * k) * (hover ? 1.0f : k + 0.2f);
        const float bv = std::min(3.0f, h * 0.12f);
        box(x, y, w, h, 0.07f, 0.06f, 0.055f, 1.0f, 4.0f);
        box(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, c[0], c[1], c[2], 1.0f, 3.0f);
        box(x + 1.0f, y + h - 1.0f - bv, w - 2.0f, bv, lr, lg, lb, 1.0f, 1.5f);
        box(x + 1.0f, y + 1.0f, bv, h - 2.0f, lr, lg, lb, 1.0f, 1.5f);
        box(x + 1.0f, y + 1.0f, w - 2.0f, bv, dr, dg, db, 1.0f, 1.5f);
        box(x + w - 1.0f - bv, y + 1.0f, bv, h - 2.0f, dr, dg, db, 1.0f, 1.5f);
        box(x + 1.0f + bv, y + 1.0f + bv, w - 2.0f - 2.0f * bv, h - 2.0f - 2.0f * bv, c[0] * 0.95f, c[1] * 0.95f, c[2] * 0.95f, 1.0f, 2.0f);
        const float ix0 = x + 2.0f + bv, ix1 = x + w - 2.0f - bv, iy0 = y + 2.0f + bv, iy1 = y + h - 2.0f - bv;
        // Plastique vieilli mais lisse : léger dégradé jauni (plus clair en haut), reflet doux, une ou deux marques d'usure
        const int bands = 4;
        for (int i = 0; i < bands; ++i)
            box(ix0, iy0 + (iy1 - iy0) * i / bands, ix1 - ix0, (iy1 - iy0) / bands + 0.5f, 0.55f, 0.45f, 0.2f, 0.07f * (i + 1) * (0.5f + 0.5f * k)); // jauni vers le bas
        box(ix0 + 2.0f, iy1 - 3.0f, ix1 - ix0 - 4.0f, 1.0f, 1.0f, 0.97f, 0.88f, 0.12f * (0.4f + 0.6f * k)); // reflet sous le biseau haut
        blotches(2, 60, ix0, ix1, iy0, iy1, 0.62f, 0.5f, 0.2f, 0.12f, 30.0f, 70.0f);                       // deux grandes zones plus jaunes
        for (int i = 0; i < 2; ++i) // un ou deux éclats sur le biseau clair
        {
            const float cl = 4.0f + 8.0f * rnd(i, 75);
            box(x + 1.0f + (w - 2.0f - cl) * rnd(i, 77), y + h - 1.0f - bv, cl, bv, c[0] * 0.85f, c[1] * 0.85f, c[2] * 0.85f, 1.0f);
        }
        if (w >= 100.0f)
            scratches(2, 50, ix0, ix1, iy0, iy1, 0.85f, 0.8f, 0.7f, 0.1f * (0.4f + 0.6f * k));
    }
    else if (style == 4)
    {
        // Étiquette noire de cassette VHS : liseré clair intérieur, bande rouge, pli, taches de café, deux morceaux de scotch jauni
        const int rows = 8;
        const float rh = h / rows;
        const float cH[3] = {0.3f, 0.07f, 0.07f}, cN[3] = {0.105f, 0.095f, 0.09f};
        const float *c = hover ? cH : cN;
        for (int r = 0; r < rows; ++r)
        {
            const float li = 3.0f * rnd(r, 11), ri = 3.0f * rnd(r, 12), shade = 0.92f + 0.16f * rnd(r, 13);
            box(x + li, y + r * rh, w - li - ri, rh + 0.5f, c[0] * shade, c[1] * shade, c[2] * shade, 1.0f);
        }
        box(x + 6.0f, y + 5.0f, w - 12.0f, 1.0f, 0.8f, 0.76f, 0.66f, 0.28f);
        box(x + 6.0f, y + h - 6.0f, w - 12.0f, 1.0f, 0.8f, 0.76f, 0.66f, 0.28f);
        box(x + 6.0f, y + 5.0f, 1.0f, h - 10.0f, 0.8f, 0.76f, 0.66f, 0.28f);
        box(x + w - 7.0f, y + 5.0f, 1.0f, h - 10.0f, 0.8f, 0.76f, 0.66f, 0.28f);
        box(x + 6.0f, y + 6.0f, 16.0f, h - 12.0f, 0.62f, 0.07f, 0.07f, 0.9f); // bande rouge
        for (int k = 0; k < 5; ++k)
            box(x + 10.0f, y + 10.0f + k * ((h - 24.0f) / 5.0f), 8.0f, 1.0f, 1.0f, 0.95f, 0.9f, 0.35f);
        blotches(10, 20, x + 26.0f, x + w - 10.0f, y + 6.0f, y + h - 6.0f, 0.3f, 0.18f, 0.07f, 0.18f, 20.0f, 60.0f);
        blotches(8, 40, x + 26.0f, x + w - 10.0f, y + 6.0f, y + h - 6.0f, 0.0f, 0.0f, 0.0f, 0.3f, 14.0f, 40.0f);
        const float fx = x + w * (0.45f + 0.2f * rnd(0, 50));
        box(fx, y + 6.0f, 1.0f, h - 12.0f, 0.0f, 0.0f, 0.0f, 0.5f);
        box(fx + 1.0f, y + 6.0f, 1.0f, h - 12.0f, 0.9f, 0.85f, 0.75f, 0.14f);
        scratches(8, 60, x + 26.0f, x + w - 10.0f, y + 8.0f, y + h - 8.0f, 0.8f, 0.76f, 0.7f, 0.2f);
        // scotch jauni, de travers, aux deux bouts
        Draw2D::setColor(0.82f * dim, 0.72f * dim, 0.4f * dim, 0.5f);
        const float tw = 44.0f, tl = 7.0f;
        Draw2D::polygon({x - 8.0f, y + h * 0.52f, x - 8.0f + tw, y + h * 0.52f + tl, x + tw - 3.0f, y + h + 6.0f, x - 11.0f, y + h + 2.0f});
        Draw2D::polygon({x + w + 8.0f, y - 2.0f, x + w + 8.0f - tw, y - 6.0f, x + w - tw + 3.0f, y + h * 0.48f, x + w + 11.0f, y + h * 0.5f});
        Draw2D::setColor(1.0f * dim, 0.95f * dim, 0.7f * dim, 0.12f);
        Draw2D::line(x - 6.0f, y + h * 0.58f, x + tw - 8.0f, y + h * 0.7f);
    }
    else if (style == 5)
    {
        // Écran de moniteur CRT : verre sombre bombé, lignes de balayage, liserés rouge et cyan décalés, pixels morts, poussière, fissure
        const float cH[3] = {0.2f, 0.04f, 0.045f}, cN[3] = {0.06f, 0.075f, 0.075f};
        const float *c = hover ? cH : cN;
        box(x, y, w, h, 0.2f, 0.2f, 0.22f, 1.0f, 12.0f); // cadre en plastique
        box(x + 3.0f, y + 3.0f, w - 6.0f, h - 6.0f, c[0], c[1], c[2], 1.0f, 10.0f);
        for (float ly = y + 4.0f; ly < y + h - 4.0f; ly += 3.0f)
            box(x + 5.0f, ly, w - 10.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.38f);
        // vignettage : le verre s'assombrit vers les bords
        for (int k = 0; k < 6; ++k)
        {
            const float a = 0.22f - k * 0.035f;
            box(x + 4.0f, y + 4.0f + k * 2.0f, w - 8.0f, 2.0f, 0.0f, 0.0f, 0.0f, a);
            box(x + 4.0f, y + h - 6.0f - k * 2.0f, w - 8.0f, 2.0f, 0.0f, 0.0f, 0.0f, a);
            box(x + 4.0f + k * 3.0f, y + 6.0f, 3.0f, h - 12.0f, 0.0f, 0.0f, 0.0f, a);
            box(x + w - 7.0f - k * 3.0f, y + 6.0f, 3.0f, h - 12.0f, 0.0f, 0.0f, 0.0f, a);
        }
        box(x + 6.0f, y + 8.0f, 2.0f, h - 16.0f, 0.1f, 0.8f, 0.9f, 0.4f); // liserés décalés, comme le titre
        box(x + w - 8.0f, y + 8.0f, 2.0f, h - 16.0f, 0.95f, 0.1f, 0.12f, 0.4f);
        box(x + 14.0f, y + h * 0.58f, w * 0.38f, h * 0.34f, 1.0f, 1.0f, 1.0f, 0.05f, 6.0f); // reflet sur le verre
        for (int i = 0; i < 4; ++i)
        {
            const float px = x + 12.0f + (w - 24.0f) * rnd(i, 3), py = y + 8.0f + (h - 16.0f) * rnd(i, 4);
            box(px, py, 2.0f, 2.0f, i & 1 ? 0.0f : 1.0f, i & 1 ? 1.0f : 0.1f, i & 1 ? 0.2f : 0.1f, 0.85f); // pixels morts
        }
        for (int i = 0; i < 26; ++i)
            box(x + 10.0f + (w - 20.0f) * rnd(i, 5), y + 8.0f + (h - 16.0f) * rnd(i, 6), 1.0f + 1.5f * rnd(i, 7), 1.0f, 0.8f, 0.8f, 0.8f, 0.3f); // poussière
        // fissure du verre depuis un coin
        const float sx = x + w - 6.0f, sy = y + h - 5.0f;
        float cx = sx, cy = sy;
        Draw2D::setLineWidth(1.0f);
        for (int s = 0; s < 6; ++s)
        {
            const float nx = cx - 7.0f - 8.0f * rnd(s, 8), ny = cy - 4.0f - 5.0f * rnd(s, 9);
            Draw2D::setColor(0.0f, 0.0f, 0.0f, 0.9f);
            Draw2D::line(cx, cy, nx, std::max(y + 5.0f, ny));
            Draw2D::setColor(0.75f * dim, 0.78f * dim, 0.8f * dim, 0.35f);
            Draw2D::line(cx + 1.0f, cy, nx + 1.0f, std::max(y + 5.0f, ny));
            cx = nx, cy = ny;
        }
    }
    else if (style == 6)
    {
        // Bande magnétique abîmée : tranches décalées comme une erreur de lecture, fantômes rouge et cyan, parasites, poussière
        const int rows = 9;
        const float rh = h / rows;
        const float cH[3] = {0.28f, 0.06f, 0.07f}, cN[3] = {0.09f, 0.085f, 0.095f};
        const float *c = hover ? cH : cN;
        for (int r = 0; r < rows; ++r)
        {
            const float big = rnd(r, 11) > 0.8f ? 14.0f : 3.0f;
            const float dx = (rnd(r, 12) - 0.5f) * 2.0f * big, shade = 0.85f + 0.3f * rnd(r, 13);
            if (big > 10.0f)
            {
                box(x + dx - 4.0f, y + r * rh, w, rh, 0.1f, 0.8f, 0.9f, 0.16f); // fantômes de la tranche arrachée
                box(x + dx + 4.0f, y + r * rh, w, rh, 0.95f, 0.1f, 0.12f, 0.16f);
            }
            box(x + std::max(0.0f, dx), y + r * rh, w - std::fabs(dx), rh + 0.5f, c[0] * shade, c[1] * shade, c[2] * shade, 1.0f);
        }
        for (int i = 0; i < 30; ++i) // traits de parasites
        {
            const float len = 8.0f + 40.0f * rnd(i, 20);
            box(x + (w - len) * rnd(i, 21), y + h * rnd(i, 22), len, 1.0f, 0.85f, 0.85f, 0.9f, 0.1f + 0.18f * rnd(i, 23));
        }
        for (int i = 0; i < 46; ++i) // neige sur les bords
        {
            const float ex = rnd(i, 24) < 0.5f ? x + 14.0f * rnd(i, 25) : x + w - 14.0f * rnd(i, 25);
            box(ex, y + h * rnd(i, 26), 1.0f + 2.0f * rnd(i, 27), 1.0f + rnd(i, 28), 0.9f, 0.9f, 0.9f, 0.5f);
        }
        for (int i = 0; i < 10; ++i)
            box(x + w * rnd(i, 29), y + h * rnd(i, 30), 2.0f, 2.0f, 0.9f, 0.88f, 0.8f, 0.5f); // poussière
        box(x, y, w, 2.0f, 0.0f, 0.0f, 0.0f, 0.5f);
        box(x, y + h - 2.0f, w, 2.0f, 0.0f, 0.0f, 0.0f, 0.5f);
    }
    else
    {
        // Béton fissuré : grain, coin haut droit et coin bas gauche arrachés, suie en bas, fissures, armature rouillée
        const int rows = 12;
        const float rh = h / rows, cut = 26.0f;
        const float cH[3] = {0.34f, 0.1f, 0.1f}, cN[3] = {0.2f, 0.2f, 0.21f};
        const float *c = hover ? cH : cN;
        for (int r = 0; r < rows; ++r)
        {
            const float ry = y + r * rh;
            // coin arraché en escalier, légèrement irrégulier
            const float ri = std::max(0.0f, cut * (float(r) - (rows - 5)) / 5.0f) * (0.8f + 0.4f * rnd(r, 1));
            const float li = std::max(0.0f, cut * (4.0f - r) / 4.0f * 1.5f) * (0.8f + 0.4f * rnd(r, 2));
            const float shade = 0.94f + 0.12f * rnd(r, 3);
            box(x + li, ry, w - li - ri, rh + 0.5f, c[0] * shade, c[1] * shade, c[2] * shade, 1.0f);
        }
        for (int i = 0; i < 90; ++i)
        {
            const float s = 2.0f + 2.5f * rnd(i, 5), gx = x + 6.0f + (w - 12.0f) * rnd(i, 6), gy = y + 4.0f + (h - 8.0f) * rnd(i, 7);
            const bool light = rnd(i, 8) > 0.5f;
            box(gx, gy, s, s, light ? 0.5f : 0.05f, light ? 0.5f : 0.05f, light ? 0.52f : 0.05f, 0.14f);
        }
        blotches(18, 80, x + 6.0f, x + w - 6.0f, y + 4.0f, y + h - 4.0f, 0.07f, 0.07f, 0.08f, 0.35f, 20.0f, 70.0f);
        // suie qui monte du bas
        for (int s = 0; s < 6; ++s)
            box(x + 26.0f, y + s * 2.0f, w - 40.0f, 2.0f, 0.03f, 0.025f, 0.025f, 0.22f - s * 0.03f);
        // fissures : trois lignes brisées qui descendent du bord, avec une branche
        Draw2D::setLineWidth(2.0f);
        for (int k = 0; k < 3; ++k)
        {
            float cx = x + 40.0f + (w - 90.0f) * (0.15f + 0.7f * rnd(k, 90) * (k + 1) / 3.0f), cy = y + h - 2.0f;
            for (int s = 0; s < 8 && cy > y + 4.0f; ++s)
            {
                const float nx = std::max(x + 6.0f, std::min(x + w - 6.0f, cx + (rnd(k * 10 + s, 91) - 0.5f) * 18.0f)), ny = cy - 7.0f - 5.0f * rnd(k * 10 + s, 92);
                Draw2D::setColor(0.03f * dim, 0.03f * dim, 0.03f * dim, 0.95f);
                Draw2D::line(cx, cy, nx, std::max(y + 3.0f, ny));
                Draw2D::setColor(0.6f * dim, 0.6f * dim, 0.62f * dim, 0.18f);
                Draw2D::line(cx + 1.5f, cy, nx + 1.5f, std::max(y + 3.0f, ny));
                if (s == 3)
                {
                    Draw2D::setColor(0.03f * dim, 0.03f * dim, 0.03f * dim, 0.9f);
                    Draw2D::setLineWidth(1.0f);
                    Draw2D::line(nx, ny, nx + 26.0f * (k & 1 ? -1.0f : 1.0f), ny - 10.0f);
                    Draw2D::setLineWidth(2.0f);
                }
                cx = nx, cy = ny;
            }
        }
        // armature rouillée qui dépasse au coin arraché
        box(x + 4.0f, y + h * 0.5f - 1.0f, 46.0f, 3.0f, 0.5f, 0.24f, 0.09f, 0.95f, 1.5f);
        box(x + 4.0f, y + h * 0.5f + 2.0f, 40.0f, 1.0f, 0.75f, 0.45f, 0.2f, 0.5f);
        box(x, y, w, 2.0f, 0.03f, 0.03f, 0.03f, 0.6f);
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
        {
            hoverNew = true;
            hoverStart = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        hoverKey = key;
        hoverSeen = true;
    }
    // Clignotement : à l'arrivée de la souris seulement, le bouton s'assombrit deux fois de suite (en 0,2 s), puis
    // reprend son aspect normal. Sans sang ni rouge de survol.
    float shade = 1.0f;
    const bool blink = hover && blinkHover && !bloodHover;
    if (blink)
    {
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const float age = (float)(now - hoverStart);
        if (age >= 0.0f && age < 0.2f && std::fmod(age, 0.1f) < 0.05f)
            shade = 0.72f;
    }
    const bool shown = hover && !bloodHover && !blink;
    const float *c = !enabled ? t.buttonDisabled : (shown ? t.buttonHover : t.button);
    const bool plate = t.plate > 0 && (t.plate >= 7 ? (w >= 20.0f && h >= 20.0f) : (w >= 100.0f && h >= 36.0f)); // les petits boutons (flèches, + / -) restent simples, sauf les touches
    if (plate)
        drawPlate(t.plate, x, y, w, h, shown, enabled, shade);
    else
        UI::drawBox(x, y, w, h, c[0] * shade, c[1] * shade, c[2] * shade, c[3], false, t.radius);
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
    if (plate && (t.plate == 7 || t.plate >= 9))
    {
        // Touche claire : texte noir, avec un reflet clair juste dessous
        UI::setColor(0.9f, 0.88f, 0.8f, enabled ? 0.45f : 0.2f);
        UI::renderText(label, x + (w - tw) / 2.0f, y + h / 2.0f - 9.0f, scale);
        UI::setColor(0.09f, 0.08f, 0.07f, enabled ? 1.0f : 0.45f);
    }
    else
    {
        if (plate)
        {
            UI::setColor(0.0f, 0.0f, 0.0f, enabled ? 0.85f : 0.4f);
            UI::renderText(label, x + (w - tw) / 2.0f + 2.0f, y + h / 2.0f - 10.0f, scale);
        }
        UI::setColor(1.0f, 1.0f, 1.0f, enabled ? 1.0f : 0.5f);
    }
    UI::renderText(label, x + (w - tw) / 2.0f, y + h / 2.0f - 8.0f, scale);
    UI::setColor(1.0f, 1.0f, 1.0f, 1.0f);
    return hover && released;
}

bool UIWidgets::arrowButton(int dir, float x, float y, float w, float h, bool enabled)
{
    bool hit = button("", x, y, w, h, enabled);
    const float ink = (currentTheme.plate == 7 || currentTheme.plate >= 9) ? 0.09f : 1.0f; // flèche noire sur les touches claires
    Draw2D::setColor(ink, ink, ink, enabled ? 1.0f : 0.35f);
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
