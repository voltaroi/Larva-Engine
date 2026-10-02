#include <GL/glew.h>
#include <GL/freeglut.h>
#include <windows.h>
#include "EditorUI.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>

namespace ui
{
    namespace col
    {
        const Color background = {0.082f, 0.082f, 0.086f, 1.0f};
        const Color panel = {0.141f, 0.141f, 0.149f, 1.0f};
        const Color panelDark = {0.102f, 0.102f, 0.110f, 1.0f};
        const Color header = {0.188f, 0.188f, 0.200f, 1.0f};
        const Color widget = {0.067f, 0.067f, 0.071f, 1.0f};
        const Color widgetHover = {0.235f, 0.235f, 0.250f, 1.0f};
        const Color widgetActive = {0.040f, 0.040f, 0.045f, 1.0f};
        const Color border = {0.239f, 0.239f, 0.255f, 1.0f};
        const Color text = {0.86f, 0.86f, 0.87f, 1.0f};
        const Color textDim = {0.56f, 0.56f, 0.59f, 1.0f};
        const Color textDisabled = {0.36f, 0.36f, 0.38f, 1.0f};
        const Color accent = {0.050f, 0.420f, 0.860f, 1.0f};
        const Color accentHover = {0.120f, 0.500f, 0.950f, 1.0f};
        const Color selection = {0.070f, 0.300f, 0.600f, 1.0f};
        const Color axisX = {0.890f, 0.230f, 0.200f, 1.0f};
        const Color axisY = {0.400f, 0.780f, 0.150f, 1.0f};
        const Color axisZ = {0.200f, 0.450f, 0.950f, 1.0f};
        const Color warning = {0.950f, 0.650f, 0.150f, 1.0f};
        const Color success = {0.300f, 0.800f, 0.400f, 1.0f};
    }

    // ---------------------------------------------------------------- Police

    struct Glyph
    {
        float u0, v0, u1, v1;
        int w, h, bearingX, bearingY;
        float advance;
    };

    struct FontAtlas
    {
        GLuint texture = 0;
        float ascent = 0.0f;
        float height = 0.0f;
        Glyph glyphs[96] = {};
    };

    static FontAtlas fonts[4];

    static bool buildAtlas(FT_Face face, int pixelSize, FontAtlas &out)
    {
        FT_Set_Pixel_Sizes(face, 0, pixelSize);
        const int atlasSize = 512;
        std::vector<unsigned char> pixels(atlasSize * atlasSize, 0);
        int penX = 1, penY = 1, rowHeight = 0;

        for (int c = 32; c < 127; c++)
        {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT))
                continue;
            FT_GlyphSlot g = face->glyph;
            int w = (int)g->bitmap.width;
            int h = (int)g->bitmap.rows;
            if (penX + w + 1 >= atlasSize)
            {
                penX = 1;
                penY += rowHeight + 1;
                rowHeight = 0;
            }
            if (penY + h + 1 >= atlasSize)
                return false;

            for (int row = 0; row < h; row++)
            {
                const unsigned char *src = g->bitmap.buffer + row * g->bitmap.pitch;
                std::copy(src, src + w, pixels.begin() + (penY + row) * atlasSize + penX);
            }

            Glyph &gl = out.glyphs[c - 32];
            gl.u0 = penX / (float)atlasSize;
            gl.v0 = penY / (float)atlasSize;
            gl.u1 = (penX + w) / (float)atlasSize;
            gl.v1 = (penY + h) / (float)atlasSize;
            gl.w = w;
            gl.h = h;
            gl.bearingX = g->bitmap_left;
            gl.bearingY = g->bitmap_top;
            gl.advance = g->advance.x / 64.0f;

            penX += w + 1;
            rowHeight = std::max(rowHeight, h);
        }

        out.ascent = face->size->metrics.ascender / 64.0f;
        out.height = face->size->metrics.height / 64.0f;

        glGenTextures(1, &out.texture);
        glBindTexture(GL_TEXTURE_2D, out.texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlasSize, atlasSize, 0, GL_ALPHA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    bool init(const char *fontPath)
    {
        FT_Library ft;
        if (FT_Init_FreeType(&ft))
            return false;
        FT_Face face;
        if (FT_New_Face(ft, fontPath, 0, &face))
        {
            FT_Done_FreeType(ft);
            return false;
        }
        const int sizes[4] = {12, 14, 18, 26};
        bool ok = true;
        for (int i = 0; i < 4; i++)
            ok = buildAtlas(face, sizes[i], fonts[i]) && ok;
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return ok;
    }

    // ---------------------------------------------------------------- Entrées

    struct Rect
    {
        float x, y, w, h;
        bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    };

    static int sw = 1, sh = 1;
    static int mx = 0, my = 0;
    static bool downState[3] = {false, false, false};
    static bool pressedAcc[3] = {false, false, false}, releasedAcc[3] = {false, false, false};
    static bool pressedNow[3] = {false, false, false}, releasedNow[3] = {false, false, false};
    static int pressX[3] = {0, 0, 0}, pressY[3] = {0, 0, 0};
    static float wheelAcc = 0.0f, wheelNow = 0.0f;
    static std::string typedAcc, typedNow;
    static std::vector<int> specialAcc, specialNow;
    static bool kCtrl = false, kShift = false, kAlt = false;
    static bool inputEventNow = false;

    static std::vector<Rect> overlaysPrev, overlaysCur;
    static std::vector<Rect> overlayStack;
    static std::vector<Rect> clipStack;

    typedef size_t WidgetId;
    static WidgetId activeId = 0;  // widget en cours de glissement
    static WidgetId focusId = 0;   // champ texte en cours de saisie
    static bool focusSeen = false;
    static std::string editBuffer;
    static size_t caret = 0;
    static bool selectAll = false;
    static int dragStartX = 0;
    static float dragStartValue = 0.0f;
    static bool dragMoved = false;
    static std::map<WidgetId, float> scrollOffsets;
    static int wantedCursor = GLUT_CURSOR_LEFT_ARROW;
    static int currentCursor = -1;

    static WidgetId makeId(const std::string &id) { return std::hash<std::string>()(id) | 1; }

    void onMouseMove(int x, int y)
    {
        mx = x;
        my = y;
    }

    void onMouseButton(int button, bool down)
    {
        if (button < 0 || button > 2)
            return;
        if (down && !downState[button])
        {
            pressedAcc[button] = true;
            pressX[button] = mx;
            pressY[button] = my;
        }
        if (!down && downState[button])
            releasedAcc[button] = true;
        downState[button] = down;
    }

    void onWheel(int direction) { wheelAcc += (float)direction; }
    void onChar(unsigned char c) { typedAcc += (char)c; }
    void onSpecialKey(int key) { specialAcc.push_back(key); }

    int mouseX() { return mx; }
    int mouseY() { return my; }
    bool mouseDown(int b) { return downState[b]; }
    bool mousePressed(int b) { return pressedNow[b]; }
    bool mouseReleased(int b) { return releasedNow[b]; }
    float wheel() { return wheelNow; }
    bool ctrl() { return kCtrl; }
    bool shift() { return kShift; }
    bool alt() { return kAlt; }
    bool hadInputEvent() { return inputEventNow; }
    int screenWidth() { return sw; }
    int screenHeight() { return sh; }

    static bool blockedByOverlay(float px, float py)
    {
        // Dans une fenêtre superposée : seules celles ouvertes après elle peuvent la masquer (non gérées)
        if (!overlayStack.empty())
            return !overlayStack.back().contains(px, py);
        for (const Rect &r : overlaysPrev)
            if (r.contains(px, py))
                return true;
        return false;
    }

    bool hover(float x, float y, float w, float h)
    {
        if (!Rect{x, y, w, h}.contains((float)mx, (float)my))
            return false;
        if (!clipStack.empty() && !clipStack.back().contains((float)mx, (float)my))
            return false;
        return !blockedByOverlay((float)mx, (float)my);
    }

    bool clicked(float x, float y, float w, float h)
    {
        return releasedNow[0] && hover(x, y, w, h) && Rect{x, y, w, h}.contains((float)pressX[0], (float)pressY[0]);
    }

    bool textFocused() { return focusId != 0; }
    void clearFocus() { focusId = 0; }
    bool widgetActive() { return activeId != 0; }

    bool mouseOverOverlay()
    {
        for (const Rect &r : overlaysPrev)
            if (r.contains((float)mx, (float)my))
                return true;
        return false;
    }

    void beginOverlay(float x, float y, float w, float h)
    {
        Rect r{x, y, w, h};
        overlaysCur.push_back(r);
        overlayStack.push_back(r);
    }

    void endOverlay()
    {
        if (!overlayStack.empty())
            overlayStack.pop_back();
    }

    void beginFrame(int width, int height)
    {
        sw = std::max(1, width);
        sh = std::max(1, height);
        for (int i = 0; i < 3; i++)
        {
            pressedNow[i] = pressedAcc[i];
            releasedNow[i] = releasedAcc[i];
            pressedAcc[i] = releasedAcc[i] = false;
        }
        wheelNow = wheelAcc;
        wheelAcc = 0.0f;
        typedNow.swap(typedAcc);
        typedAcc.clear();
        specialNow.swap(specialAcc);
        specialAcc.clear();
        // Modificateurs lus directement : GLUT ne les donne que pendant ses callbacks
        kCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        kShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        kAlt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        inputEventNow = pressedNow[0] || pressedNow[1] || pressedNow[2] || releasedNow[0] || releasedNow[1] ||
                        releasedNow[2] || wheelNow != 0.0f || !typedNow.empty() || !specialNow.empty();

        overlaysPrev.swap(overlaysCur);
        overlaysCur.clear();
        overlayStack.clear();
        clipStack.clear();
        focusSeen = false;
        wantedCursor = GLUT_CURSOR_LEFT_ARROW;
        setup2D();
    }

    const std::string &typedChars() { return typedNow; }
    const std::vector<int> &specialKeys() { return specialNow; }

    void setup2D()
    {
        glViewport(0, 0, sw, sh);
        glDisable(GL_SCISSOR_TEST);
        glUseProgram(0);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, sw, sh, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void endFrame()
    {
        if (focusId != 0 && !focusSeen)
            focusId = 0; // le champ n'est plus affiché
        if (!downState[0] && !releasedNow[0])
            activeId = 0;
        if (wantedCursor != currentCursor)
        {
            glutSetCursor(wantedCursor);
            currentCursor = wantedCursor;
        }
        glDisable(GL_SCISSOR_TEST);
    }

    void setCursor(int cursor) { wantedCursor = cursor; }

    // ---------------------------------------------------------------- Dessin

    static void applyClip()
    {
        if (clipStack.empty())
        {
            glDisable(GL_SCISSOR_TEST);
            return;
        }
        const Rect &r = clipStack.back();
        glEnable(GL_SCISSOR_TEST);
        glScissor((int)std::floor(r.x), (int)std::floor(sh - (r.y + r.h)), (int)std::ceil(std::max(0.0f, r.w)), (int)std::ceil(std::max(0.0f, r.h)));
    }

    void pushClip(float x, float y, float w, float h)
    {
        Rect r{x, y, w, h};
        if (!clipStack.empty())
        {
            const Rect &p = clipStack.back();
            float x0 = std::max(r.x, p.x), y0 = std::max(r.y, p.y);
            float x1 = std::min(r.x + r.w, p.x + p.w), y1 = std::min(r.y + r.h, p.y + p.h);
            r = {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
        }
        clipStack.push_back(r);
        applyClip();
    }

    void popClip()
    {
        if (!clipStack.empty())
            clipStack.pop_back();
        applyClip();
    }

    static void setColor(const Color &c) { glColor4f(c.r, c.g, c.b, c.a); }

    void rect(float x, float y, float w, float h, const Color &c)
    {
        setColor(c);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        glEnd();
    }

    void rectOutline(float x, float y, float w, float h, const Color &c, float t)
    {
        rect(x, y, w, t, c);
        rect(x, y + h - t, w, t, c);
        rect(x, y + t, t, h - 2 * t, c);
        rect(x + w - t, y + t, t, h - 2 * t, c);
    }

    void line(float x0, float y0, float x1, float y1, const Color &c, float thickness)
    {
        setColor(c);
        glLineWidth(thickness);
        glBegin(GL_LINES);
        glVertex2f(x0, y0);
        glVertex2f(x1, y1);
        glEnd();
        glLineWidth(1.0f);
    }

    static const FontAtlas &atlas(Font f) { return fonts[(int)f]; }

    float lineHeight(Font f) { return std::ceil(atlas(f).height); }

    float textWidth(const std::string &s, Font f)
    {
        const FontAtlas &a = atlas(f);
        float w = 0.0f;
        for (unsigned char c : s)
        {
            if (c >= 32 && c < 127)
                w += a.glyphs[c - 32].advance;
        }
        return w;
    }

    float text(const std::string &s, float x, float y, const Color &c, Font f)
    {
        const FontAtlas &a = atlas(f);
        if (!a.texture)
            return 0.0f;
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, a.texture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        setColor(c);
        float penX = std::round(x);
        float baseline = std::round(y + a.ascent);
        glBegin(GL_QUADS);
        for (unsigned char ch : s)
        {
            if (ch < 32 || ch >= 127)
                continue;
            const Glyph &g = a.glyphs[ch - 32];
            if (g.w > 0 && g.h > 0)
            {
                float gx = std::round(penX + g.bearingX);
                float gy = baseline - g.bearingY;
                glTexCoord2f(g.u0, g.v0);
                glVertex2f(gx, gy);
                glTexCoord2f(g.u1, g.v0);
                glVertex2f(gx + g.w, gy);
                glTexCoord2f(g.u1, g.v1);
                glVertex2f(gx + g.w, gy + g.h);
                glTexCoord2f(g.u0, g.v1);
                glVertex2f(gx, gy + g.h);
            }
            penX += g.advance;
        }
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
        return penX - x;
    }

    void textClipped(const std::string &s, float x, float y, float maxWidth, const Color &c, Font f)
    {
        if (textWidth(s, f) <= maxWidth)
        {
            text(s, x, y, c, f);
            return;
        }
        std::string cut = s;
        float dots = textWidth("..", f);
        while (!cut.empty() && textWidth(cut, f) + dots > maxWidth)
            cut.pop_back();
        text(cut + "..", x, y, c, f);
    }

    void textCentered(const std::string &s, float x, float y, float w, float h, const Color &c, Font f)
    {
        float tw = textWidth(s, f);
        text(s, x + (w - tw) * 0.5f, y + (h - lineHeight(f)) * 0.5f, c, f);
    }

    std::string formatFloat(float v, int decimals)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
        std::string s = buf;
        size_t dot = s.find('.');
        if (dot != std::string::npos)
        {
            size_t last = s.find_last_not_of('0');
            if (last == dot)
                last--;
            s.erase(last + 1);
        }
        if (s == "-0")
            s = "0";
        return s;
    }

    // ---------------------------------------------------------------- Widgets

    bool button(const std::string &label, float x, float y, float w, float h, bool selected, bool enabled)
    {
        bool hov = enabled && hover(x, y, w, h);
        bool held = hov && downState[0] && Rect{x, y, w, h}.contains((float)pressX[0], (float)pressY[0]);
        Color bg = selected ? (hov ? col::accentHover : col::accent) : (held ? col::widgetActive : (hov ? col::widgetHover : col::header));
        rect(x, y, w, h, bg);
        rectOutline(x, y, w, h, selected ? col::accentHover : col::border);
        pushClip(x + 2, y, w - 4, h);
        textCentered(label, x, y, w, h, enabled ? col::text : col::textDisabled, Font::Normal);
        popClip();
        return enabled && clicked(x, y, w, h);
    }

    bool checkbox(const std::string &label, bool &value, float x, float y)
    {
        const float s = 16.0f;
        float w = s + 8.0f + textWidth(label);
        bool hov = hover(x, y, w, s);
        rect(x, y, s, s, hov ? col::widgetHover : col::widget);
        rectOutline(x, y, s, s, value ? col::accent : col::border);
        if (value)
            rect(x + 4, y + 4, s - 8, s - 8, col::accentHover);
        text(label, x + s + 8.0f, y + (s - lineHeight()) * 0.5f, col::text);
        if (clicked(x, y, w, s))
        {
            value = !value;
            return true;
        }
        return false;
    }

    // Copie / collage via le presse-papiers Windows
    static void copyToClipboard(const std::string &s)
    {
        if (!OpenClipboard(nullptr))
            return;
        EmptyClipboard();
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
        if (mem)
        {
            char *dst = (char *)GlobalLock(mem);
            std::copy(s.begin(), s.end(), dst);
            dst[s.size()] = 0;
            GlobalUnlock(mem);
            SetClipboardData(CF_TEXT, mem);
        }
        CloseClipboard();
    }

    static std::string clipboardText()
    {
        std::string out;
        if (!OpenClipboard(nullptr))
            return out;
        HANDLE data = GetClipboardData(CF_TEXT);
        if (data)
        {
            const char *src = (const char *)GlobalLock(data);
            if (src)
                out = src;
            GlobalUnlock(data);
        }
        CloseClipboard();
        out.erase(std::remove_if(out.begin(), out.end(), [](char c)
                                 { return (unsigned char)c < 32 || (unsigned char)c > 126; }),
                  out.end());
        return out;
    }

    // 0 : rien, 1 : validé (Entrée), 2 : annulé (Échap)
    static int processEditing()
    {
        for (unsigned char c : typedNow)
        {
            if (c == 13)
                return 1;
            if (c == 27)
                return 2;
            if (c == 1) // Ctrl+A
            {
                selectAll = true;
                continue;
            }
            if (c == 3) // Ctrl+C
            {
                copyToClipboard(editBuffer);
                continue;
            }
            if (c == 22) // Ctrl+V
            {
                std::string paste = clipboardText();
                if (selectAll)
                {
                    editBuffer.clear();
                    caret = 0;
                    selectAll = false;
                }
                editBuffer.insert(caret, paste);
                caret += paste.size();
                continue;
            }
            if (c == 8 || c == 127)
            {
                if (selectAll)
                {
                    editBuffer.clear();
                    caret = 0;
                }
                else if (c == 8 && caret > 0)
                    editBuffer.erase(--caret, 1);
                else if (c == 127 && caret < editBuffer.size())
                    editBuffer.erase(caret, 1);
                selectAll = false;
                continue;
            }
            if (c >= 32 && c < 127)
            {
                if (selectAll)
                {
                    editBuffer.clear();
                    caret = 0;
                    selectAll = false;
                }
                editBuffer.insert(editBuffer.begin() + caret, (char)c);
                caret++;
            }
        }
        for (int key : specialNow)
        {
            selectAll = false;
            if (key == GLUT_KEY_LEFT && caret > 0)
                caret--;
            else if (key == GLUT_KEY_RIGHT && caret < editBuffer.size())
                caret++;
            else if (key == GLUT_KEY_HOME)
                caret = 0;
            else if (key == GLUT_KEY_END)
                caret = editBuffer.size();
        }
        return 0;
    }

    static void drawEditBox(float x, float y, float w, float h)
    {
        rect(x, y, w, h, col::widgetActive);
        rectOutline(x, y, w, h, col::accent);
        pushClip(x + 4, y, w - 8, h);
        float ty = y + (h - lineHeight()) * 0.5f;
        float before = textWidth(editBuffer.substr(0, caret));
        // Décale le texte pour garder le curseur visible
        float shift = std::max(0.0f, before - (w - 14.0f));
        float tx = x + 6.0f - shift;
        if (selectAll && !editBuffer.empty())
            rect(tx - 1, ty, textWidth(editBuffer) + 2, lineHeight(), col::selection);
        text(editBuffer, tx, ty, col::text);
        if ((GetTickCount() / 500) % 2 == 0)
            rect(tx + before, ty, 1.0f, lineHeight(), col::text);
        popClip();
    }

    bool textField(const std::string &idStr, std::string &value, float x, float y, float w, float h, const char *placeholder)
    {
        WidgetId id = makeId(idStr);
        bool hov = hover(x, y, w, h);
        if (hov)
            wantedCursor = GLUT_CURSOR_TEXT;

        if (focusId != id && clicked(x, y, w, h))
        {
            focusId = id;
            editBuffer = value;
            caret = editBuffer.size();
            selectAll = true;
        }

        if (focusId == id)
        {
            focusSeen = true;
            int r = processEditing();
            bool commit = r == 1 || (r == 0 && pressedNow[0] && !hov);
            if (r == 2)
                focusId = 0;
            else if (commit)
            {
                focusId = 0;
                if (editBuffer != value)
                {
                    value = editBuffer;
                    drawEditBox(x, y, w, h);
                    return true;
                }
            }
            if (focusId == id)
            {
                drawEditBox(x, y, w, h);
                return false;
            }
        }

        rect(x, y, w, h, hov ? col::widgetHover : col::widget);
        rectOutline(x, y, w, h, col::border);
        pushClip(x + 4, y, w - 8, h);
        float ty = y + (h - lineHeight()) * 0.5f;
        if (value.empty() && placeholder)
            text(placeholder, x + 6.0f, ty, col::textDisabled);
        else
            text(value, x + 6.0f, ty, col::text);
        popClip();
        return false;
    }

    bool floatField(const std::string &idStr, float &value, float x, float y, float w, float h, float dragSpeed,
                    const Color *accentColor, float minValue, float maxValue)
    {
        WidgetId id = makeId(idStr);
        bool hov = hover(x, y, w, h);
        bool changed = false;

        if (focusId == id)
        {
            focusSeen = true;
            int r = processEditing();
            bool commit = r == 1 || (r == 0 && pressedNow[0] && !hov);
            if (r == 2)
                focusId = 0;
            else if (commit)
            {
                focusId = 0;
                char *end = nullptr;
                double parsed = std::strtod(editBuffer.c_str(), &end);
                if (end != editBuffer.c_str())
                {
                    float nv = std::max(minValue, std::min(maxValue, (float)parsed));
                    if (nv != value)
                    {
                        value = nv;
                        changed = true;
                    }
                }
            }
            if (focusId == id)
            {
                drawEditBox(x, y, w, h);
                return false;
            }
        }

        if (hov && activeId == 0)
            wantedCursor = GLUT_CURSOR_LEFT_RIGHT;
        if (pressedNow[0] && hov && activeId == 0)
        {
            activeId = id;
            dragStartX = mx;
            dragStartValue = value;
            dragMoved = false;
        }
        if (activeId == id)
        {
            wantedCursor = GLUT_CURSOR_LEFT_RIGHT;
            int dx = mx - dragStartX;
            if (std::abs(dx) > 2)
                dragMoved = true;
            if (dragMoved && downState[0])
            {
                float speed = dragSpeed * (kShift ? 0.1f : 1.0f);
                float nv = std::max(minValue, std::min(maxValue, dragStartValue + dx * speed));
                if (nv != value)
                {
                    value = nv;
                    changed = true;
                }
            }
            if (releasedNow[0])
            {
                activeId = 0;
                if (!dragMoved)
                {
                    focusId = id;
                    focusSeen = true;
                    editBuffer = formatFloat(value);
                    caret = editBuffer.size();
                    selectAll = true;
                }
            }
        }

        bool active = activeId == id;
        rect(x, y, w, h, active ? col::widgetActive : (hov ? col::widgetHover : col::widget));
        rectOutline(x, y, w, h, active ? col::accent : col::border);
        if (accentColor)
            rect(x + 1, y + 1, 3, h - 2, *accentColor);
        pushClip(x + 4, y, w - 8, h);
        text(formatFloat(value), x + 8.0f, y + (h - lineHeight()) * 0.5f, col::text);
        popClip();
        return changed;
    }

    bool listItem(const std::string &label, float x, float y, float w, float h, bool selected, const Color *dot)
    {
        bool hov = hover(x, y, w, h);
        if (selected)
            rect(x, y, w, h, col::selection);
        else if (hov)
            rect(x, y, w, h, col::widgetHover);
        float tx = x + 8.0f;
        if (dot)
        {
            rect(x + 8.0f, y + h * 0.5f - 4.0f, 8.0f, 8.0f, *dot);
            tx += 16.0f;
        }
        textClipped(label, tx, y + (h - lineHeight()) * 0.5f, w - (tx - x) - 4.0f, selected ? col::text : col::text);
        return clicked(x, y, w, h);
    }

    float scrollArea(const std::string &idStr, float x, float y, float w, float h, float contentHeight)
    {
        WidgetId id = makeId(idStr);
        float &offset = scrollOffsets[id];
        float maxOffset = std::max(0.0f, contentHeight - h);
        if (hover(x, y, w, h) && wheelNow != 0.0f)
            offset -= wheelNow * 48.0f;

        if (maxOffset > 0.0f)
        {
            const float barW = 6.0f;
            float thumbH = std::max(24.0f, h * h / contentHeight);
            float bx = x + w - barW - 2.0f;
            if (pressedNow[0] && hover(bx - 2, y, barW + 4, h))
            {
                activeId = id;
                dragStartX = my;
                dragStartValue = offset;
            }
            if (activeId == id && downState[0])
                offset = dragStartValue + (my - dragStartX) * (contentHeight - h) / std::max(1.0f, h - thumbH);
            offset = std::max(0.0f, std::min(maxOffset, offset));
            float ty = y + (h - thumbH) * (offset / maxOffset);
            rect(bx, ty, barW, thumbH, activeId == id ? col::accent : col::border);
        }
        offset = std::max(0.0f, std::min(maxOffset, offset));
        return offset;
    }
}
