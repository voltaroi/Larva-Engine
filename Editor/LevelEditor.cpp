#include <GL/glew.h>
#include <GL/freeglut.h>
#include <windows.h>
#include "LevelEditor.h"
#include "EditorUI.h"
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <glm/glm.hpp>

static const float TOOLBAR_H = 44.0f;
static const float STATUS_H = 26.0f;
static const float LEFT_W = 250.0f;
static const float RIGHT_W = 350.0f;
static const float ROW_H = 24.0f;
static const float GRID_SIZES[] = {0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
static const int GRID_COUNT = sizeof(GRID_SIZES) / sizeof(GRID_SIZES[0]);
static const float ROTATE_SNAP = 15.0f;
static const float SCALE_SNAP = 0.1f;

static const ui::Color COLOR_START = {0.35f, 0.85f, 0.35f, 1.0f};
static const ui::Color COLOR_MARKER = {0.95f, 0.8f, 0.2f, 1.0f};
static const ui::Color COLOR_MESH = {0.6f, 0.6f, 0.65f, 1.0f};
static const ui::Color COLOR_SELECT = {1.0f, 0.55f, 0.1f, 1.0f};
static const ui::Color COLOR_HOT = {1.0f, 0.9f, 0.2f, 1.0f};

static const ui::Color &axisColor(int axis)
{
    return axis == 0 ? ui::col::axisX : (axis == 1 ? ui::col::axisY : ui::col::axisZ);
}

static void glColor(const ui::Color &c, float alpha = -1.0f)
{
    glColor4f(c.r, c.g, c.b, alpha < 0.0f ? c.a : alpha);
}

static glm::vec3 toVec(const float v[3]) { return glm::vec3(v[0], v[1], v[2]); }

static void wireBox(const float mn[3], const float mx[3])
{
    const float x[2] = {mn[0], mx[0]}, y[2] = {mn[1], mx[1]}, z[2] = {mn[2], mx[2]};
    glBegin(GL_LINES);
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
        {
            glVertex3f(x[0], y[i], z[j]);
            glVertex3f(x[1], y[i], z[j]);
            glVertex3f(x[i], y[0], z[j]);
            glVertex3f(x[i], y[1], z[j]);
            glVertex3f(x[i], y[j], z[0]);
            glVertex3f(x[i], y[j], z[1]);
        }
    glEnd();
}

// ============================================================ Ouverture

bool LevelEditor::open(const std::string &root, const project::Info &project, const std::string &levelPath, std::string &message)
{
    close();
    rootDir = root;
    proj = project;
    levelRelPath = levelPath;

    // Les chemins des modèles et des shaders sont relatifs aux Assets, comme dans le game.pak
    project::ensureShaders(rootDir, proj.assetsDir);
    SetCurrentDirectoryA(proj.assetsDir.c_str());

    Level loaded;
    if (!loaded.loadFromFile(levelRelPath))
    {
        message = levelRelPath + ": " + loaded.lastError();
        return false;
    }
    scene.setLevel(loaded);
    modelAssets = project::listModels(proj.assetsDir);

    selected = -1;
    undoStack.clear();
    redoStack.clear();
    committedState = savedState = scene.level.saveToString();
    pending = Pending::None;
    modelPopupOpen = false;
    dragHandle = HANDLE_NONE;
    flying = panning = false;
    opened = true;

    // Caméra derrière le point d'apparition s'il existe
    const LevelEntity *start = scene.level.playerStart();
    camYaw = 0.0f;
    camPitch = -0.35f;
    if (start)
    {
        camPos[0] = start->position[0];
        camPos[1] = start->position[1] + 4.0f;
        camPos[2] = start->position[2] - 8.0f;
    }
    else
    {
        camPos[0] = 0.0f;
        camPos[1] = 6.0f;
        camPos[2] = -14.0f;
    }
    lastFrame = std::chrono::steady_clock::now();
    setStatus("Opened " + levelRelPath);
    return true;
}

void LevelEditor::close()
{
    scene.unloadModels();
    scene.level.clear();
    opened = false;
}

bool LevelEditor::isDirty() const
{
    return opened && committedState != savedState;
}

bool LevelEditor::save()
{
    checkpoint();
    if (!scene.level.saveToFile(levelRelPath))
    {
        setStatus("ERROR: cannot write " + levelRelPath);
        return false;
    }
    savedState = committedState;
    setStatus("Saved " + levelRelPath);
    return true;
}

void LevelEditor::setStatus(const std::string &message)
{
    statusMessage = message;
    statusTime = std::chrono::steady_clock::now();
}

// ============================================================ Image

LevelEditor::Action LevelEditor::frame(int w, int h)
{
    auto now = std::chrono::steady_clock::now();
    frameDt = std::min(0.1f, std::chrono::duration<float>(now - lastFrame).count());
    lastFrame = now;

    Action action = Action::None;
    layout(w, h);
    if (pending == Pending::None)
        handleShortcuts(action);
    updateCamera();
    render3D();
    viewportInput();

    ui::setup2D();
    drawViewportOverlay();
    drawToolbar(action);
    drawContent();
    float rightX = w - RIGHT_W;
    float panelH = h - TOOLBAR_H - STATUS_H;
    float outlinerH = std::floor(panelH * 0.4f);
    drawOutliner(rightX, TOOLBAR_H, RIGHT_W, outlinerH);
    drawDetails(rightX, TOOLBAR_H + outlinerH, RIGHT_W, panelH - outlinerH);
    drawStatusBar();
    drawModelPopup();
    drawContentDrag();
    drawModal(action);

    // Une modification terminée (clic relâché, saisie validée...) devient une étape d'annulation
    if (ui::hadInputEvent() && !ui::widgetActive() && !ui::textFocused() && dragHandle == HANDLE_NONE && !ui::mouseDown(0))
        checkpoint();
    return action;
}

void LevelEditor::layout(int w, int h)
{
    viewport = {LEFT_W, TOOLBAR_H, std::max(1.0f, w - LEFT_W - RIGHT_W), std::max(1.0f, h - TOOLBAR_H - STATUS_H)};
}

void LevelEditor::handleShortcuts(Action &action)
{
    if (ui::textFocused() || flying)
        return;
    for (unsigned char c : ui::typedChars())
    {
        switch (c)
        {
        case 19: // Ctrl+S
            save();
            break;
        case 26: // Ctrl+Z
            undo();
            break;
        case 25: // Ctrl+Y
            redo();
            break;
        case 4: // Ctrl+D
            duplicateSelected(true);
            break;
        case 3: // Ctrl+C
            if (selected >= 0)
            {
                clipboard = scene.level.entities[selected];
                hasClipboard = true;
                setStatus("Copied " + clipboard.name);
            }
            break;
        case 22: // Ctrl+V
            if (hasClipboard)
            {
                LevelEntity e = clipboard;
                e.name = scene.level.uniqueName(e.name);
                e.position[0] += std::max(1.0f, gridSize());
                addEntity(e);
            }
            break;
        case 127: // Suppr
            deleteSelected();
            break;
        case 27: // Échap
            select(-1);
            break;
        case 'w':
        case 'W':
            gizmoMode = GizmoMode::Translate;
            break;
        case 'e':
        case 'E':
            gizmoMode = GizmoMode::Rotate;
            break;
        case 'r':
        case 'R':
            gizmoMode = GizmoMode::Scale;
            break;
        case ' ':
            gizmoMode = gizmoMode == GizmoMode::Translate ? GizmoMode::Rotate : (gizmoMode == GizmoMode::Rotate ? GizmoMode::Scale : GizmoMode::Translate);
            break;
        case 'f':
        case 'F':
            focusSelection();
            break;
        }
    }
    for (int key : ui::specialKeys())
    {
        if (key == GLUT_KEY_END)
            dropToFloor();
#ifdef GLUT_KEY_DELETE
        if (key == GLUT_KEY_DELETE)
            deleteSelected();
#endif
    }
    (void)action;
}

// ============================================================ Caméra

void LevelEditor::cameraVectors(float forward[3], float right[3]) const
{
    forward[0] = std::cos(camPitch) * std::sin(camYaw);
    forward[1] = std::sin(camPitch);
    forward[2] = std::cos(camPitch) * std::cos(camYaw);
    right[0] = -std::cos(camYaw);
    right[1] = 0.0f;
    right[2] = std::sin(camYaw);
}

static bool windowHasFocus()
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static bool keyDown(UINT scanCode)
{
    // Touches lues par position physique : ZQSD en AZERTY = WASD en QWERTY
    UINT vk = MapVirtualKeyA(scanCode, MAPVK_VSC_TO_VK);
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void LevelEditor::updateCamera()
{
    bool inViewport = ui::hover(viewport.x, viewport.y, viewport.w, viewport.h);
    float fwd[3], right[3];
    cameraVectors(fwd, right);

    // Clic droit maintenu : vue libre + déplacement (comme Unreal)
    if (!flying && ui::mousePressed(2) && inViewport && dragHandle == HANDLE_NONE)
    {
        POINT p;
        GetCursorPos(&p);
        flyAnchorX = p.x;
        flyAnchorY = p.y;
        flying = true;
        ui::clearFocus();
    }
    if (flying && !ui::mouseDown(2))
        flying = false;

    if (flying)
    {
        ui::setCursor(GLUT_CURSOR_NONE);
        POINT p;
        GetCursorPos(&p);
        int dx = p.x - flyAnchorX, dy = p.y - flyAnchorY;
        SetCursorPos(flyAnchorX, flyAnchorY);
        camYaw -= dx * 0.0035f;
        camPitch = std::max(-1.55f, std::min(1.55f, camPitch - dy * 0.0035f));
        if (ui::wheel() != 0.0f)
        {
            camSpeed = std::max(0.5f, std::min(200.0f, camSpeed * std::pow(1.25f, ui::wheel())));
            setStatus("Camera speed " + ui::formatFloat(camSpeed, 1));
        }
    }

    // ZQSD (WASD en QWERTY) : déplacement de la caméra, même sans clic droit.
    // Montée / descente (A/E) seulement en vol, E servant sinon au mode rotation.
    bool canMove = flying || (!ui::textFocused() && !ui::ctrl() && !ui::alt() && pending == Pending::None);
    if (canMove && windowHasFocus())
    {
        cameraVectors(fwd, right);
        float move[3] = {0, 0, 0};
        float f = (keyDown(0x11) ? 1.0f : 0.0f) - (keyDown(0x1F) ? 1.0f : 0.0f); // Z/W - S
        float r = (keyDown(0x20) ? 1.0f : 0.0f) - (keyDown(0x1E) ? 1.0f : 0.0f); // D - Q/A
        float u = flying ? (keyDown(0x12) ? 1.0f : 0.0f) - (keyDown(0x10) ? 1.0f : 0.0f) : 0.0f; // E - A/Q
        for (int i = 0; i < 3; i++)
            move[i] = fwd[i] * f + right[i] * r + (i == 1 ? u : 0.0f);
        float speed = camSpeed * ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 3.0f : 1.0f);
        for (int i = 0; i < 3; i++)
            camPos[i] += move[i] * speed * frameDt;
    }

    // Clic molette maintenu : panoramique
    if (!panning && ui::mousePressed(1) && inViewport)
    {
        panning = true;
        lastMouseX = ui::mouseX();
        lastMouseY = ui::mouseY();
    }
    if (panning && !ui::mouseDown(1))
        panning = false;
    if (panning)
    {
        float dx = (float)(ui::mouseX() - lastMouseX), dy = (float)(ui::mouseY() - lastMouseY);
        lastMouseX = ui::mouseX();
        lastMouseY = ui::mouseY();
        glm::vec3 f = toVec(fwd), r = toVec(right);
        glm::vec3 up = glm::normalize(glm::cross(r, f));
        float k = 0.0025f * camSpeed;
        glm::vec3 delta = r * (-dx * k) + up * (dy * k);
        for (int i = 0; i < 3; i++)
            camPos[i] += delta[i];
    }

    // Molette : avancer / reculer
    if (!flying && inViewport && ui::wheel() != 0.0f && !ui::mouseOverOverlay())
    {
        float step = ui::wheel() * std::max(0.5f, camSpeed * 0.2f);
        for (int i = 0; i < 3; i++)
            camPos[i] += fwd[i] * step;
    }
}

void LevelEditor::focusSelection()
{
    float mn[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, mx[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    bool any = false;
    for (int i = 0; i < (int)scene.level.entities.size(); i++)
    {
        if (selected >= 0 && i != selected)
            continue;
        AABB b = scene.worldBounds(scene.level.entities[i]);
        mn[0] = std::min(mn[0], b.minX), mx[0] = std::max(mx[0], b.maxX);
        mn[1] = std::min(mn[1], b.minY), mx[1] = std::max(mx[1], b.maxY);
        mn[2] = std::min(mn[2], b.minZ), mx[2] = std::max(mx[2], b.maxZ);
        any = true;
    }
    if (!any)
        return;
    float fwd[3], right[3];
    cameraVectors(fwd, right);
    float radius = 0.0f;
    for (int i = 0; i < 3; i++)
        radius = std::max(radius, (mx[i] - mn[i]) * 0.5f);
    float dist = radius * 2.2f + 1.5f;
    for (int i = 0; i < 3; i++)
        camPos[i] = (mn[i] + mx[i]) * 0.5f - fwd[i] * dist;
}

// ============================================================ Rendu 3D

void LevelEditor::render3D()
{
    int sh = ui::screenHeight();
    int vx = (int)viewport.x, vw = (int)viewport.w, vh = (int)viewport.h;
    int vy = sh - (int)(viewport.y + viewport.h);
    glViewportRect[0] = vx;
    glViewportRect[1] = vy;
    glViewportRect[2] = vw;
    glViewportRect[3] = vh;

    glViewport(vx, vy, vw, vh);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vx, vy, vw, vh);
    const float *sky = scene.level.skyColor;
    glClearColor(sky[0], sky[1], sky[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST); // sinon la carte d'ombres ne serait effacée qu'en partie

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0, (double)vw / (double)std::max(1, vh), 0.05, 3000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float fwd[3], right[3];
    cameraVectors(fwd, right);
    gluLookAt(camPos[0], camPos[1], camPos[2], camPos[0] + fwd[0], camPos[1] + fwd[1], camPos[2] + fwd[2], 0.0, 1.0, 0.0);
    glGetDoublev(GL_MODELVIEW_MATRIX, viewMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);

    scene.render();

    glUseProgram(0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    drawGrid();
    drawHelpers();
    drawGizmo();

    glDisable(GL_DEPTH_TEST);
}

void LevelEditor::drawGrid()
{
    glDepthMask(GL_FALSE);
    glLineWidth(1.0f);
    const int half = 60;
    float cx = std::round(camPos[0]), cz = std::round(camPos[2]);
    // Chaque ligne part du point le plus proche de la caméra et s'estompe vers ses extrémités
    auto fadedLine = [&](float x0, float z0, float x1, float z1, float mx, float mz, float alpha)
    {
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
        glVertex3f(x0, 0.0f, z0);
        glColor4f(0.0f, 0.0f, 0.0f, alpha);
        glVertex3f(mx, 0.0f, mz);
        glVertex3f(mx, 0.0f, mz);
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
        glVertex3f(x1, 0.0f, z1);
    };
    glBegin(GL_LINES);
    for (int i = -half; i <= half; i++)
    {
        float x = cx + i, z = cz + i;
        float fade = 1.0f - std::fabs((float)i) / half;
        float ax = std::fmod(std::fabs(x), 10.0f) < 0.01f ? 0.4f : 0.16f;
        float az = std::fmod(std::fabs(z), 10.0f) < 0.01f ? 0.4f : 0.16f;
        fadedLine(x, cz - half, x, cz + half, x, camPos[2], ax * fade);
        fadedLine(cx - half, z, cx + half, z, camPos[0], z, az * fade);
    }
    glEnd();

    // Axes du monde : X rouge, Z bleu
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor(ui::col::axisX, 0.8f);
    glVertex3f(cx - half, 0.002f, 0.0f);
    glVertex3f(cx + half, 0.002f, 0.0f);
    glColor(ui::col::axisZ, 0.8f);
    glVertex3f(0.0f, 0.002f, cz - half);
    glVertex3f(0.0f, 0.002f, cz + half);
    glEnd();
    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
}

void LevelEditor::drawHelpers()
{
    for (int i = 0; i < (int)scene.level.entities.size(); i++)
    {
        const LevelEntity &e = scene.level.entities[i];
        bool isMesh = e.type == LevelEntity::Type::Mesh;
        bool missing = isMesh && !scene.isModelLoaded(e.model);
        if (isMesh && !missing)
            continue;
        if (!e.visible && i != selected)
            continue;

        float mat[16], mn[3], mx[3];
        LevelScene::entityMatrix(e, mat);
        scene.localBounds(e, mn, mx);
        glPushMatrix();
        glMultMatrixf(mat);
        glLineWidth(2.0f);
        if (missing)
        {
            glColor4f(1.0f, 0.0f, 1.0f, 1.0f);
            wireBox(mn, mx);
        }
        else if (e.type == LevelEntity::Type::PlayerStart)
        {
            glColor(COLOR_START);
            wireBox(mn, mx);
            // Flèche de direction (rotation Y)
            glBegin(GL_LINES);
            glVertex3f(0.0f, 0.0f, 0.0f);
            glVertex3f(0.0f, 0.0f, 1.2f);
            glVertex3f(0.0f, 0.0f, 1.2f);
            glVertex3f(0.25f, 0.0f, 0.9f);
            glVertex3f(0.0f, 0.0f, 1.2f);
            glVertex3f(-0.25f, 0.0f, 0.9f);
            glEnd();
        }
        else
        {
            glColor(COLOR_MARKER);
            glBegin(GL_LINES);
            for (int a = 0; a < 3; a++)
            {
                float p[3] = {0, 0, 0};
                p[a] = mx[a] * 1.6f;
                glVertex3f(-p[0], -p[1], -p[2]);
                glVertex3f(p[0], p[1], p[2]);
            }
            glEnd();
            wireBox(mn, mx);
        }
        glPopMatrix();
    }

    // Lumière : soleil et direction
    const float *lp = scene.level.lightPosition, *lt = scene.level.lightTarget;
    glLineWidth(1.5f);
    glColor4f(1.0f, 0.85f, 0.2f, 0.9f);
    glBegin(GL_LINE_LOOP);
    for (int k = 0; k < 24; k++)
    {
        float a = k / 24.0f * 6.2831853f;
        glVertex3f(lp[0] + std::cos(a) * 0.5f, lp[1], lp[2] + std::sin(a) * 0.5f);
    }
    glEnd();
    glBegin(GL_LINE_LOOP);
    for (int k = 0; k < 24; k++)
    {
        float a = k / 24.0f * 6.2831853f;
        glVertex3f(lp[0] + std::cos(a) * 0.5f, lp[1] + std::sin(a) * 0.5f, lp[2]);
    }
    glEnd();
    glColor4f(1.0f, 0.85f, 0.2f, 0.15f);
    glBegin(GL_LINES);
    glVertex3fv(lp);
    glVertex3fv(lt);
    glEnd();

    // Sélection : boîte visible à travers les objets
    if (selected >= 0)
    {
        const LevelEntity &e = scene.level.entities[selected];
        float mat[16], mn[3], mx[3];
        LevelScene::entityMatrix(e, mat);
        scene.localBounds(e, mn, mx);
        glPushMatrix();
        glMultMatrixf(mat);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
        glColor(COLOR_SELECT, 0.35f);
        wireBox(mn, mx);
        glEnable(GL_DEPTH_TEST);
        glLineWidth(2.0f);
        glColor(COLOR_SELECT);
        wireBox(mn, mx);
        glPopMatrix();
    }
    glLineWidth(1.0f);
}

// ============================================================ Gizmo

float LevelEditor::gizmoLength() const
{
    if (selected < 0)
        return 1.0f;
    const float *p = scene.level.entities[selected].position;
    float d = glm::length(toVec(p) - toVec(camPos));
    // Taille constante à l'écran (environ 120 pixels)
    float pixels = 120.0f / std::max(1.0f, viewport.h);
    return std::max(0.05f, d * 1.4f * pixels);
}

bool LevelEditor::project(const float p[3], float &sx, float &sy) const
{
    double wx, wy, wz;
    if (!gluProject(p[0], p[1], p[2], viewMatrix, projMatrix, glViewportRect, &wx, &wy, &wz))
        return false;
    sx = (float)wx;
    sy = (float)(ui::screenHeight() - wy);
    return wz > 0.0 && wz < 1.0;
}

void LevelEditor::mouseRay(int mx, int my, float origin[3], float dir[3]) const
{
    double winY = ui::screenHeight() - my;
    double nx, ny, nz, fx, fy, fz;
    gluUnProject(mx, winY, 0.0, viewMatrix, projMatrix, glViewportRect, &nx, &ny, &nz);
    gluUnProject(mx, winY, 1.0, viewMatrix, projMatrix, glViewportRect, &fx, &fy, &fz);
    glm::vec3 d = glm::normalize(glm::vec3((float)(fx - nx), (float)(fy - ny), (float)(fz - nz)));
    origin[0] = (float)nx;
    origin[1] = (float)ny;
    origin[2] = (float)nz;
    dir[0] = d.x;
    dir[1] = d.y;
    dir[2] = d.z;
}

static float distToSegment(float px, float py, float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay;
    float len2 = dx * dx + dy * dy;
    float t = len2 > 0.0f ? std::max(0.0f, std::min(1.0f, ((px - ax) * dx + (py - ay) * dy) / len2)) : 0.0f;
    float cx = ax + dx * t - px, cy = ay + dy * t - py;
    return std::sqrt(cx * cx + cy * cy);
}

// Coin du carré de déplacement plan (normale n) : u, v en fraction de la longueur du gizmo
static void planeCorner(const float o[3], int n, float L, float u, float v, float out[3])
{
    int a = (n + 1) % 3, b = (n + 2) % 3;
    out[0] = o[0];
    out[1] = o[1];
    out[2] = o[2];
    out[a] += u * L;
    out[b] += v * L;
}

int LevelEditor::pickHandle(int mx, int my) const
{
    if (selected < 0)
        return HANDLE_NONE;
    const float *o = scene.level.entities[selected].position;
    float L = gizmoLength();
    float ox, oy;
    if (!project(o, ox, oy))
        return HANDLE_NONE;
    const float threshold = 9.0f;
    float best = threshold;
    int handle = HANDLE_NONE;

    if (gizmoMode == GizmoMode::Rotate)
    {
        for (int a = 0; a < 3; a++)
        {
            int u = (a + 1) % 3, v = (a + 2) % 3;
            float prevX = 0, prevY = 0;
            bool prevOk = false;
            for (int k = 0; k <= 48; k++)
            {
                float ang = k / 48.0f * 6.2831853f;
                float p[3] = {o[0], o[1], o[2]};
                p[u] += std::cos(ang) * L;
                p[v] += std::sin(ang) * L;
                float sx, sy;
                bool ok = project(p, sx, sy);
                if (ok && prevOk)
                {
                    float d = distToSegment((float)mx, (float)my, prevX, prevY, sx, sy);
                    if (d < best)
                    {
                        best = d;
                        handle = a;
                    }
                }
                prevX = sx;
                prevY = sy;
                prevOk = ok;
            }
        }
        return handle;
    }

    if (gizmoMode == GizmoMode::Scale)
    {
        float d = std::sqrt((mx - ox) * (mx - ox) + (my - oy) * (my - oy));
        if (d < 11.0f)
            return HANDLE_UNIFORM;
    }

    for (int a = 0; a < 3; a++)
    {
        float tip[3] = {o[0], o[1], o[2]};
        tip[a] += L;
        float tx, ty;
        if (!project(tip, tx, ty))
            continue;
        float d = distToSegment((float)mx, (float)my, ox, oy, tx, ty);
        if (d < best)
        {
            best = d;
            handle = a;
        }
    }
    if (handle != HANDLE_NONE || gizmoMode != GizmoMode::Translate)
        return handle;

    // Carrés de déplacement dans un plan
    for (int n = 0; n < 3; n++)
    {
        float c[4][3], s[4][2];
        planeCorner(o, n, L, 0.2f, 0.2f, c[0]);
        planeCorner(o, n, L, 0.45f, 0.2f, c[1]);
        planeCorner(o, n, L, 0.45f, 0.45f, c[2]);
        planeCorner(o, n, L, 0.2f, 0.45f, c[3]);
        bool ok = true;
        for (int k = 0; k < 4; k++)
            ok = project(c[k], s[k][0], s[k][1]) && ok;
        if (!ok)
            continue;
        int sign = 0;
        bool inside = true;
        for (int k = 0; k < 4 && inside; k++)
        {
            const float *p0 = s[k], *p1 = s[(k + 1) % 4];
            float cross = (p1[0] - p0[0]) * (my - p0[1]) - (p1[1] - p0[1]) * (mx - p0[0]);
            int sg = cross > 0 ? 1 : -1;
            if (sign == 0)
                sign = sg;
            else if (sg != sign)
                inside = false;
        }
        if (inside)
            return HANDLE_PLANE_X + n;
    }
    return HANDLE_NONE;
}

bool LevelEditor::axisParam(int axis, const float origin[3], const float dir[3], float &param) const
{
    // Point de l'axe (passant par la position de départ) le plus proche du rayon souris
    glm::vec3 a(0.0f);
    a[axis] = 1.0f;
    glm::vec3 d = toVec(dir);
    glm::vec3 w0 = toVec(dragStartPos) - toVec(origin);
    float b = glm::dot(a, d), c = glm::dot(d, d);
    float dd = glm::dot(a, w0), e = glm::dot(d, w0);
    float denom = c - b * b;
    if (std::fabs(denom) < 1e-5f)
        return false;
    param = (b * e - c * dd) / denom;
    return true;
}

bool LevelEditor::planeHit(int n, const float origin[3], const float dir[3], float hit[3]) const
{
    if (std::fabs(dir[n]) < 1e-5f)
        return false;
    float t = (dragStartPos[n] - origin[n]) / dir[n];
    if (t < 0.0f)
        return false;
    for (int i = 0; i < 3; i++)
        hit[i] = origin[i] + dir[i] * t;
    return true;
}

void LevelEditor::beginGizmoDrag(int handle, int mx, int my)
{
    // Alt + glisser : déplace une copie (comme Unreal)
    if (gizmoMode == GizmoMode::Translate && ui::alt())
        duplicateSelected(false);

    LevelEntity &e = scene.level.entities[selected];
    for (int i = 0; i < 3; i++)
    {
        dragStartPos[i] = e.position[i];
        dragStartRot[i] = e.rotation[i];
        dragStartScale[i] = e.scale[i];
    }
    dragStartMouseX = mx;
    dragStartMouseY = my;
    dragStartLength = gizmoLength();

    float o[3], d[3];
    mouseRay(mx, my, o, d);
    if (handle < 3 && gizmoMode != GizmoMode::Rotate && !axisParam(handle, o, d, dragStartParam))
        return;
    if (handle >= HANDLE_PLANE_X && handle < HANDLE_UNIFORM && !planeHit(handle - HANDLE_PLANE_X, o, d, dragStartHit))
        return;
    dragHandle = handle;
}

void LevelEditor::updateGizmoDrag(int mx, int my)
{
    LevelEntity &e = scene.level.entities[selected];
    float o[3], d[3];
    mouseRay(mx, my, o, d);
    float g = gridSize();

    if (gizmoMode == GizmoMode::Translate)
    {
        if (dragHandle < 3)
        {
            float param;
            if (!axisParam(dragHandle, o, d, param))
                return;
            float v = dragStartPos[dragHandle] + (param - dragStartParam);
            e.position[dragHandle] = snapEnabled ? snap(v, g) : v;
        }
        else
        {
            int n = dragHandle - HANDLE_PLANE_X;
            float hit[3];
            if (!planeHit(n, o, d, hit))
                return;
            for (int i = 0; i < 3; i++)
            {
                if (i == n)
                    continue;
                float v = dragStartPos[i] + (hit[i] - dragStartHit[i]);
                e.position[i] = snapEnabled ? snap(v, g) : v;
            }
        }
    }
    else if (gizmoMode == GizmoMode::Rotate)
    {
        float delta = ((mx - dragStartMouseX) - (my - dragStartMouseY)) * 0.5f;
        float v = dragStartRot[dragHandle] + delta;
        if (snapEnabled)
            v = snap(v, ROTATE_SNAP);
        v = std::fmod(v, 360.0f);
        e.rotation[dragHandle] = v;
    }
    else
    {
        if (dragHandle == HANDLE_UNIFORM)
        {
            float factor = std::max(0.01f, 1.0f + (mx - dragStartMouseX) / 150.0f);
            for (int i = 0; i < 3; i++)
            {
                float v = dragStartScale[i] * factor;
                e.scale[i] = snapEnabled ? std::max(SCALE_SNAP, snap(v, SCALE_SNAP)) : v;
            }
        }
        else
        {
            float param;
            if (!axisParam(dragHandle, o, d, param))
                return;
            float factor = 1.0f + (param - dragStartParam) / dragStartLength;
            float v = dragStartScale[dragHandle] * factor;
            if (snapEnabled)
                v = snap(v, SCALE_SNAP);
            if (std::fabs(v) < 0.01f)
                v = v < 0.0f ? -0.01f : 0.01f;
            e.scale[dragHandle] = v;
        }
    }
}

static void axisFrame(int a, int &u, int &v)
{
    u = (a + 1) % 3;
    v = (a + 2) % 3;
}

void LevelEditor::drawGizmo()
{
    if (selected < 0)
        return;
    const float *o = scene.level.entities[selected].position;
    float L = gizmoLength();
    glDisable(GL_DEPTH_TEST);

    auto handleColor = [&](int h) -> ui::Color
    {
        if (h == dragHandle || (dragHandle == HANDLE_NONE && h == hoveredHandle))
            return COLOR_HOT;
        if (h < 3)
            return axisColor(h);
        if (h == HANDLE_UNIFORM)
            return ui::Color{0.9f, 0.9f, 0.9f, 1.0f};
        return axisColor(h - HANDLE_PLANE_X);
    };

    if (gizmoMode == GizmoMode::Rotate)
    {
        glLineWidth(3.0f);
        for (int a = 0; a < 3; a++)
        {
            int u, v;
            axisFrame(a, u, v);
            glColor(handleColor(a));
            glBegin(GL_LINE_LOOP);
            for (int k = 0; k < 64; k++)
            {
                float ang = k / 64.0f * 6.2831853f;
                float p[3] = {o[0], o[1], o[2]};
                p[u] += std::cos(ang) * L;
                p[v] += std::sin(ang) * L;
                glVertex3fv(p);
            }
            glEnd();
        }
    }
    else
    {
        // Axes
        for (int a = 0; a < 3; a++)
        {
            int u, v;
            axisFrame(a, u, v);
            ui::Color c = handleColor(a);
            float end = gizmoMode == GizmoMode::Translate ? 0.8f : 0.92f;
            float p0[3] = {o[0], o[1], o[2]}, p1[3] = {o[0], o[1], o[2]};
            p1[a] += L * end;
            glLineWidth(3.0f);
            glColor(c);
            glBegin(GL_LINES);
            glVertex3fv(p0);
            glVertex3fv(p1);
            glEnd();

            if (gizmoMode == GizmoMode::Translate)
            {
                // Cône au bout
                float tip[3] = {o[0], o[1], o[2]};
                tip[a] += L;
                float r = L * 0.06f;
                glBegin(GL_TRIANGLES);
                for (int k = 0; k < 12; k++)
                {
                    float a0 = k / 12.0f * 6.2831853f, a1 = (k + 1) / 12.0f * 6.2831853f;
                    float b0[3] = {p1[0], p1[1], p1[2]}, b1[3] = {p1[0], p1[1], p1[2]};
                    b0[u] += std::cos(a0) * r;
                    b0[v] += std::sin(a0) * r;
                    b1[u] += std::cos(a1) * r;
                    b1[v] += std::sin(a1) * r;
                    glVertex3fv(tip);
                    glVertex3fv(b0);
                    glVertex3fv(b1);
                    glVertex3fv(p1);
                    glVertex3fv(b1);
                    glVertex3fv(b0);
                }
                glEnd();
            }
            else
            {
                float s = L * 0.07f;
                float c0[3] = {p1[0] - s, p1[1] - s, p1[2] - s}, c1[3] = {p1[0] + s, p1[1] + s, p1[2] + s};
                c0[a] += s;
                c1[a] += s;
                glLineWidth(2.0f);
                wireBox(c0, c1);
            }
        }

        if (gizmoMode == GizmoMode::Translate)
        {
            for (int n = 0; n < 3; n++)
            {
                float c[4][3];
                planeCorner(o, n, L, 0.2f, 0.2f, c[0]);
                planeCorner(o, n, L, 0.45f, 0.2f, c[1]);
                planeCorner(o, n, L, 0.45f, 0.45f, c[2]);
                planeCorner(o, n, L, 0.2f, 0.45f, c[3]);
                ui::Color col = handleColor(HANDLE_PLANE_X + n);
                bool hot = (HANDLE_PLANE_X + n) == dragHandle || (dragHandle == HANDLE_NONE && HANDLE_PLANE_X + n == hoveredHandle);
                glColor(col, hot ? 0.6f : 0.3f);
                glBegin(GL_QUADS);
                for (int k = 0; k < 4; k++)
                    glVertex3fv(c[k]);
                glEnd();
                glLineWidth(1.0f);
                glColor(col, 0.9f);
                glBegin(GL_LINE_LOOP);
                for (int k = 0; k < 4; k++)
                    glVertex3fv(c[k]);
                glEnd();
            }
        }
        else
        {
            float s = L * 0.09f;
            float c0[3] = {o[0] - s, o[1] - s, o[2] - s}, c1[3] = {o[0] + s, o[1] + s, o[2] + s};
            glLineWidth(2.0f);
            glColor(handleColor(HANDLE_UNIFORM));
            wireBox(c0, c1);
        }
    }
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}

// ============================================================ Souris dans le viewport

void LevelEditor::viewportInput()
{
    int mx = ui::mouseX(), my = ui::mouseY();
    bool inViewport = ui::hover(viewport.x, viewport.y, viewport.w, viewport.h) && pending == Pending::None;

    if (selected >= (int)scene.level.entities.size())
        selected = -1;

    if (dragHandle == HANDLE_NONE)
        hoveredHandle = (selected >= 0 && inViewport && !flying && !panning) ? pickHandle(mx, my) : HANDLE_NONE;

    if (ui::mousePressed(0) && inViewport && !flying)
    {
        ui::clearFocus();
        if (hoveredHandle != HANDLE_NONE)
            beginGizmoDrag(hoveredHandle, mx, my);
        else
        {
            viewportPressed = true;
            pressMouseX = mx;
            pressMouseY = my;
        }
    }

    if (dragHandle != HANDLE_NONE)
    {
        if (selected < 0)
            dragHandle = HANDLE_NONE;
        else
        {
            updateGizmoDrag(mx, my);
            if (!ui::mouseDown(0))
                dragHandle = HANDLE_NONE;
        }
    }

    if (viewportPressed && ui::mouseReleased(0))
    {
        viewportPressed = false;
        if (std::abs(mx - pressMouseX) < 5 && std::abs(my - pressMouseY) < 5 && inViewport)
        {
            float o[3], d[3], t;
            mouseRay(mx, my, o, d);
            select(scene.raycast(o, d, t, -1, true));
        }
    }
}

// ============================================================ Édition

float LevelEditor::gridSize() const { return GRID_SIZES[gridIndex]; }

float LevelEditor::snap(float v, float step) const
{
    return step > 0.0f ? std::round(v / step) * step : v;
}

void LevelEditor::select(int index)
{
    if (index < -1 || index >= (int)scene.level.entities.size())
        index = -1;
    selected = index;
    modelPopupOpen = false;
}

int LevelEditor::addEntity(const LevelEntity &e)
{
    scene.level.entities.push_back(e);
    scene.refreshModels();
    select((int)scene.level.entities.size() - 1);
    return selected;
}

void LevelEditor::deleteSelected()
{
    if (selected < 0)
        return;
    setStatus("Deleted " + scene.level.entities[selected].name);
    scene.level.entities.erase(scene.level.entities.begin() + selected);
    select(-1);
}

void LevelEditor::duplicateSelected(bool offset)
{
    if (selected < 0)
        return;
    LevelEntity copy = scene.level.entities[selected];
    copy.name = scene.level.uniqueName(copy.name);
    if (offset)
        copy.position[0] += std::max(1.0f, gridSize());
    // Insérée juste après l'original
    scene.level.entities.insert(scene.level.entities.begin() + selected + 1, copy);
    select(selected + 1);
}

void LevelEditor::dropToFloor()
{
    if (selected < 0)
        return;
    LevelEntity &e = scene.level.entities[selected];
    AABB b = scene.worldBounds(e);
    float o[3] = {(b.minX + b.maxX) * 0.5f, b.minY + 0.01f, (b.minZ + b.maxZ) * 0.5f};
    float d[3] = {0.0f, -1.0f, 0.0f};
    float t;
    if (scene.raycast(o, d, t, selected) >= 0)
        e.position[1] -= t - 0.01f;
    else if (b.minY > 0.0f)
        e.position[1] -= b.minY;
    setStatus("Dropped " + e.name + " to floor");
}

LevelEntity LevelEditor::makeEntity(int contentIndex) const
{
    LevelEntity e;
    if (contentIndex == -1)
    {
        e.type = LevelEntity::Type::PlayerStart;
        e.name = scene.level.uniqueName("PlayerStart");
    }
    else if (contentIndex == -3)
    {
        e.type = LevelEntity::Type::Marker;
        e.name = scene.level.uniqueName("Marker");
    }
    else
    {
        e.model = modelAssets[contentIndex];
        std::string stem = e.model.substr(e.model.find_last_of('/') + 1);
        stem = stem.substr(0, stem.find_last_of('.'));
        if (!stem.empty())
            stem[0] = (char)std::toupper((unsigned char)stem[0]);
        e.name = scene.level.uniqueName(stem.empty() ? "Mesh" : stem);
    }
    return e;
}

void LevelEditor::placementPoint(int mx, int my, float out[3]) const
{
    float o[3], d[3], t;
    mouseRay(mx, my, o, d);
    if (scene.raycast(o, d, t) >= 0 && t < 500.0f)
    {
        for (int i = 0; i < 3; i++)
            out[i] = o[i] + d[i] * t;
        return;
    }
    if (d[1] < -1e-4f)
    {
        t = -o[1] / d[1];
        if (t < 500.0f)
        {
            for (int i = 0; i < 3; i++)
                out[i] = o[i] + d[i] * t;
            return;
        }
    }
    for (int i = 0; i < 3; i++)
        out[i] = o[i] + d[i] * 10.0f;
}

void LevelEditor::spawnInFront(int contentIndex)
{
    int cx = (int)(viewport.x + viewport.w * 0.5f), cy = (int)(viewport.y + viewport.h * 0.5f);
    LevelEntity e = makeEntity(contentIndex);
    float p[3];
    placementPoint(cx, cy, p);
    if (snapEnabled)
    {
        p[0] = snap(p[0], gridSize());
        p[2] = snap(p[2], gridSize());
    }
    for (int i = 0; i < 3; i++)
        e.position[i] = p[i];
    int index = addEntity(e);
    // Pose l'objet sur la surface visée
    AABB b = scene.worldBounds(scene.level.entities[index]);
    scene.level.entities[index].position[1] += p[1] - b.minY;
    setStatus("Added " + e.name);
}

// ============================================================ Historique

void LevelEditor::checkpoint()
{
    std::string now = scene.level.saveToString();
    if (now == committedState)
        return;
    undoStack.push_back({committedState, selected});
    if (undoStack.size() > 200)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
    committedState = now;
}

void LevelEditor::restore(const Snapshot &s)
{
    scene.level.loadFromString(s.data);
    scene.refreshModels();
    committedState = s.data;
    select(s.selected);
    dragHandle = HANDLE_NONE;
}

void LevelEditor::undo()
{
    checkpoint();
    if (undoStack.empty())
        return;
    Snapshot s = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back({committedState, selected});
    restore(s);
    setStatus("Undo");
}

void LevelEditor::redo()
{
    checkpoint();
    if (redoStack.empty())
        return;
    Snapshot s = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back({committedState, selected});
    restore(s);
    setStatus("Redo");
}

// ============================================================ Panneaux

void LevelEditor::drawToolbar(Action &action)
{
    float w = (float)ui::screenWidth();
    ui::rect(0, 0, w, TOOLBAR_H, ui::col::panel);
    ui::rect(0, TOOLBAR_H - 1, w, 1, ui::col::background);

    float x = 8.0f, y = 8.0f, h = 28.0f;
    auto sep = [&]()
    {
        ui::rect(x + 4, y + 2, 1, h - 4, ui::col::border);
        x += 13.0f;
    };
    auto btn = [&](const std::string &label, float bw, bool sel = false, bool enabled = true)
    {
        bool r = ui::button(label, x, y, bw, h, sel, enabled);
        x += bw + 4.0f;
        return r;
    };

    if (btn("Save", 64, false))
        save();
    if (btn("Levels", 70))
    {
        if (isDirty())
            pending = Pending::LeaveConfirm;
        else
            action = Action::BackToLevels;
    }
    sep();
    if (btn("Undo", 60, false, !undoStack.empty() || committedState != scene.level.saveToString()))
        undo();
    if (btn("Redo", 60, false, !redoStack.empty()))
        redo();
    sep();
    if (btn("Move", 64, gizmoMode == GizmoMode::Translate))
        gizmoMode = GizmoMode::Translate;
    if (btn("Rotate", 68, gizmoMode == GizmoMode::Rotate))
        gizmoMode = GizmoMode::Rotate;
    if (btn("Scale", 60, gizmoMode == GizmoMode::Scale))
        gizmoMode = GizmoMode::Scale;
    sep();
    if (btn("Snap", 56, snapEnabled))
        snapEnabled = !snapEnabled;
    if (btn("Grid " + ui::formatFloat(gridSize(), 2), 84))
        gridIndex = (gridIndex + 1) % GRID_COUNT;
    sep();
    if (btn("Focus", 64, false, true))
        focusSelection();
    if (btn("Duplicate", 90, false, selected >= 0))
        duplicateSelected(true);
    if (btn("Delete", 68, false, selected >= 0))
        deleteSelected();

    std::string title = proj.name + "  /  " + scene.level.name + (isDirty() ? "  *" : "");
    float tw = ui::textWidth(title, ui::Font::Large);
    float tx = std::max(x + 12.0f, w - tw - 16.0f);
    ui::text(title, tx, (TOOLBAR_H - ui::lineHeight(ui::Font::Large)) * 0.5f, isDirty() ? ui::col::warning : ui::col::text, ui::Font::Large);
}

static void panelHeader(const std::string &title, float x, float y, float w)
{
    ui::rect(x, y, w, 28, ui::col::header);
    ui::text(title, x + 10, y + (28 - ui::lineHeight()) * 0.5f, ui::col::text);
    ui::rect(x, y + 27, w, 1, ui::col::background);
}

static void sectionLabel(const std::string &title, float x, float y, float w)
{
    ui::text(title, x, y, ui::col::textDim, ui::Font::Small);
    float tw = ui::textWidth(title, ui::Font::Small);
    ui::rect(x + tw + 8, y + ui::lineHeight(ui::Font::Small) * 0.5f, std::max(0.0f, w - tw - 8), 1, ui::col::border);
}

void LevelEditor::drawContent()
{
    float x = 0, y = TOOLBAR_H, w = LEFT_W, h = ui::screenHeight() - TOOLBAR_H - STATUS_H;
    ui::rect(x, y, w, h, ui::col::panel);
    ui::rect(x + w - 1, y, 1, h, ui::col::background);
    panelHeader("Content", x, y, w - 1);
    if (ui::button("Refresh", x + w - 78, y + 3, 70, 22))
    {
        modelAssets = project::listModels(proj.assetsDir);
        scene.unloadModels();
        scene.refreshModels();
        setStatus("Content refreshed");
    }

    float cy = y + 36;
    sectionLabel("ACTORS", x + 10, cy, w - 20);
    cy += 20;

    // -1 PlayerStart, -3 Marker, >= 0 modèles
    auto item = [&](int index, const std::string &label, const ui::Color *dot, float iy)
    {
        bool hov = ui::hover(x + 4, iy, w - 8, ROW_H);
        if (ui::mousePressed(0) && hov)
        {
            contentDragIndex = index;
            contentDragging = false;
        }
        bool sel = contentDragIndex == index && ui::mouseDown(0);
        if (ui::listItem(label, x + 4, iy, w - 8, ROW_H, sel, dot) && !contentDragging)
            spawnInFront(index);
    };

    item(-1, "Player Start", &COLOR_START, cy);
    cy += ROW_H;
    item(-3, "Marker", &COLOR_MARKER, cy);
    cy += ROW_H + 10;

    sectionLabel("MODELS", x + 10, cy, w - 20);
    cy += 20;
    float listH = y + h - cy - 30;
    float offset = ui::scrollArea("content.models", x, cy, w - 1, listH, modelAssets.size() * ROW_H);
    ui::pushClip(x, cy, w - 10, listH);
    for (int i = 0; i < (int)modelAssets.size(); i++)
    {
        float iy = cy + i * ROW_H - offset;
        if (iy + ROW_H < cy || iy > cy + listH)
            continue;
        item(i, modelAssets[i], &COLOR_MESH, iy);
    }
    if (modelAssets.empty())
        ui::text("No model in Assets", x + 12, cy + 4, ui::col::textDim);
    ui::popClip();
    ui::text("Click: add  |  Drag: drop in view", x + 10, y + h - 22, ui::col::textDim, ui::Font::Small);

    // Glisser-déposer vers le viewport
    if (contentDragIndex != -2)
    {
        if (ui::mouseDown(0) && !contentDragging)
        {
            if (ui::mouseX() > LEFT_W + 4)
                contentDragging = true;
        }
        if (ui::mouseReleased(0))
        {
            if (contentDragging && ui::hover(viewport.x, viewport.y, viewport.w, viewport.h))
            {
                LevelEntity e = makeEntity(contentDragIndex);
                float p[3];
                placementPoint(ui::mouseX(), ui::mouseY(), p);
                if (snapEnabled)
                {
                    p[0] = snap(p[0], gridSize());
                    p[2] = snap(p[2], gridSize());
                }
                for (int i = 0; i < 3; i++)
                    e.position[i] = p[i];
                int index = addEntity(e);
                AABB b = scene.worldBounds(scene.level.entities[index]);
                scene.level.entities[index].position[1] += p[1] - b.minY;
                setStatus("Added " + e.name);
            }
            contentDragIndex = -2;
            contentDragging = false;
        }
        else if (!ui::mouseDown(0))
        {
            contentDragIndex = -2;
            contentDragging = false;
        }
    }
}

void LevelEditor::drawContentDrag()
{
    if (!contentDragging || contentDragIndex == -2)
        return;
    std::string label = contentDragIndex == -1 ? "Player Start" : (contentDragIndex == -3 ? "Marker" : modelAssets[contentDragIndex]);
    float tw = ui::textWidth(label) + 16;
    float x = (float)ui::mouseX() + 14, y = (float)ui::mouseY() + 10;
    ui::rect(x, y, tw, 24, ui::col::accent);
    ui::text(label, x + 8, y + (24 - ui::lineHeight()) * 0.5f, ui::col::text);
}

void LevelEditor::drawOutliner(float x, float y, float w, float h)
{
    ui::rect(x, y, w, h, ui::col::panel);
    ui::rect(x, y, 1, h, ui::col::background);
    panelHeader("Outliner  (" + std::to_string(scene.level.entities.size()) + ")", x, y, w);

    ui::textField("outliner.filter", outlinerFilter, x + 8, y + 34, w - 16, 24, "Search...");

    float ly = y + 64, lh = h - 64 - 4;
    std::string filter = outlinerFilter;
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
    std::vector<int> rows;
    for (int i = 0; i < (int)scene.level.entities.size(); i++)
    {
        std::string n = scene.level.entities[i].name;
        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
        if (filter.empty() || n.find(filter) != std::string::npos)
            rows.push_back(i);
    }

    float offset = ui::scrollArea("outliner.list", x, ly, w, lh, rows.size() * 22.0f);
    ui::pushClip(x, ly, w - 10, lh);
    for (size_t r = 0; r < rows.size(); r++)
    {
        int i = rows[r];
        LevelEntity &e = scene.level.entities[i];
        float iy = ly + r * 22.0f - offset;
        if (iy + 22 < ly || iy > ly + lh)
            continue;
        const ui::Color *dot = e.type == LevelEntity::Type::PlayerStart ? &COLOR_START : (e.type == LevelEntity::Type::Marker ? &COLOR_MARKER : &COLOR_MESH);

        // Œil de visibilité à droite
        float ex = x + w - 40, ey = iy + 4;
        bool eyeHover = ui::hover(ex, ey, 22, 14);
        if (ui::clicked(ex, ey, 22, 14))
            e.visible = !e.visible;

        bool clicked = ui::listItem(e.name, x + 4, iy, w - 50, 22, i == selected, dot);
        ui::Color eyeCol = eyeHover ? ui::col::text : ui::col::textDim;
        ui::rectOutline(ex + 2, ey + 1, 18, 12, eyeCol);
        if (e.visible)
            ui::rect(ex + 8, ey + 4, 6, 6, eyeCol);
        else
            ui::line(ex + 2, ey + 13, ex + 20, ey + 1, eyeCol);

        if (clicked)
        {
            unsigned long now = GetTickCount();
            if (lastOutlinerClickIndex == i && now - lastOutlinerClickTime < 400)
            {
                select(i);
                focusSelection();
            }
            else
                select(i);
            lastOutlinerClickIndex = i;
            lastOutlinerClickTime = now;
        }
    }
    ui::popClip();
}

void LevelEditor::drawDetails(float x, float y, float w, float h)
{
    ui::rect(x, y, w, h, ui::col::panel);
    ui::rect(x, y, 1, h, ui::col::background);
    ui::rect(x, y, w, 1, ui::col::background);
    bool hasSel = selected >= 0 && selected < (int)scene.level.entities.size();
    panelHeader(hasSel ? "Details" : "Level settings", x, y, w);

    float top = y + 28, areaH = h - 28;
    float offset = ui::scrollArea("details.scroll", x, top, w, areaH, detailsContentHeight);
    ui::pushClip(x, top, w - 10, areaH);
    float cy = top + 10 - offset;
    if (hasSel)
        drawEntityDetails(scene.level.entities[selected], x + 10, cy, w - 30);
    else
        drawLevelSettings(x + 10, cy, w - 30);
    ui::popClip();
    detailsContentHeight = cy + offset - top + 10;
}

// Ligne "label | X Y Z" de trois champs numériques
static bool vec3Row(const std::string &id, const std::string &label, float v[3], float x, float &y, float w, float speed,
                    float minV = -1e9f, float maxV = 1e9f, bool colors = true)
{
    const float labelW = 86.0f;
    ui::text(label, x, y + 4, ui::col::textDim);
    float fw = (w - labelW - 8) / 3.0f;
    bool changed = false;
    for (int i = 0; i < 3; i++)
    {
        const ui::Color *c = colors ? &axisColor(i) : nullptr;
        changed |= ui::floatField(id + "." + std::to_string(i), v[i], x + labelW + i * (fw + 4), y, fw, 24, speed, c, minV, maxV);
    }
    y += 30;
    return changed;
}

void LevelEditor::drawLevelSettings(float x, float &y, float w)
{
    Level &lvl = scene.level;
    const float labelW = 86.0f;
    ui::text("Name", x, y + 4, ui::col::textDim);
    std::string name = lvl.name;
    if (ui::textField("level.name", name, x + labelW, y, w - labelW, 24) && !name.empty())
        lvl.name = name;
    y += 34;

    sectionLabel("ENVIRONMENT", x, y, w);
    y += 22;
    vec3Row("level.sky", "Sky color", lvl.skyColor, x, y, w, 0.005f, 0.0f, 1.0f, false);
    ui::rect(x + labelW, y, w - labelW, 10, ui::Color{lvl.skyColor[0], lvl.skyColor[1], lvl.skyColor[2], 1.0f});
    y += 18;
    vec3Row("level.lightpos", "Light pos", lvl.lightPosition, x, y, w, 0.05f);
    vec3Row("level.lighttarget", "Light target", lvl.lightTarget, x, y, w, 0.05f);
    ui::text("Shadow area", x, y + 4, ui::col::textDim);
    ui::floatField("level.shadow", lvl.shadowArea, x + labelW, y, (w - labelW) / 3.0f, 24, 0.1f, nullptr, 1.0f, 500.0f);
    y += 40;

    sectionLabel("INFO", x, y, w);
    y += 22;
    int meshes = 0, starts = 0;
    for (const auto &e : lvl.entities)
    {
        meshes += e.type == LevelEntity::Type::Mesh;
        starts += e.type == LevelEntity::Type::PlayerStart;
    }
    ui::text(std::to_string(lvl.entities.size()) + " entities, " + std::to_string(meshes) + " meshes", x, y, ui::col::text);
    y += 22;
    if (starts == 0)
    {
        ui::text("No Player Start in this level", x, y, ui::col::warning);
        y += 22;
    }
    ui::text("File: " + levelRelPath, x, y, ui::col::textDim);
    y += 22;
    ui::text("Select an entity to edit it.", x, y, ui::col::textDim);
    y += 22;
}

void LevelEditor::drawEntityDetails(LevelEntity &e, float x, float &y, float w)
{
    const float labelW = 86.0f;

    ui::text("Name", x, y + 4, ui::col::textDim);
    std::string name = e.name;
    if (ui::textField("entity.name", name, x + labelW, y, w - labelW, 24) && !name.empty() && name != e.name)
    {
        LevelEntity *other = scene.level.find(name);
        e.name = (other && other != &e) ? scene.level.uniqueName(name) : name;
    }
    y += 30;

    ui::text("Type", x, y + 4, ui::col::textDim);
    const char *types[3] = {"Mesh", "Start", "Marker"};
    const LevelEntity::Type typeValues[3] = {LevelEntity::Type::Mesh, LevelEntity::Type::PlayerStart, LevelEntity::Type::Marker};
    float bw = (w - labelW - 8) / 3.0f;
    for (int i = 0; i < 3; i++)
    {
        if (ui::button(types[i], x + labelW + i * (bw + 4), y, bw, 24, e.type == typeValues[i]))
        {
            e.type = typeValues[i];
            scene.refreshModels();
        }
    }
    y += 30;

    if (e.type == LevelEntity::Type::Mesh)
    {
        ui::text("Model", x, y + 4, ui::col::textDim);
        bool loaded = scene.isModelLoaded(e.model);
        float mx = x + labelW, mw = w - labelW;
        bool hov = ui::hover(mx, y, mw, 24);
        ui::rect(mx, y, mw, 24, hov ? ui::col::widgetHover : ui::col::widget);
        ui::rectOutline(mx, y, mw, 24, modelPopupOpen ? ui::col::accent : ui::col::border);
        ui::pushClip(mx + 4, y, mw - 24, 24);
        ui::text(e.model.empty() ? "(none)" : e.model, mx + 6, y + 4, loaded ? ui::col::text : ui::col::warning);
        ui::popClip();
        {
            float ax = mx + mw - 16, ay = y + 10;
            ui::Color c = ui::col::textDim;
            glColor4f(c.r, c.g, c.b, c.a);
            glBegin(GL_TRIANGLES);
            glVertex2f(ax, ay);
            glVertex2f(ax + 9, ay);
            glVertex2f(ax + 4.5f, ay + 5);
            glEnd();
        }
        if (ui::clicked(mx, y, mw, 24))
        {
            modelPopupOpen = !modelPopupOpen;
            modelPopupAnchor = {mx, y + 24, mw, 0};
        }
        if (modelPopupOpen)
            modelPopupAnchor = {mx, y + 24, mw, 0};
        y += 30;
        if (!loaded && !e.model.empty())
        {
            ui::text("Model not found or failed to load", x + labelW, y, ui::col::warning, ui::Font::Small);
            y += 20;
        }
    }

    y += 6;
    sectionLabel("TRANSFORM", x, y, w);
    y += 22;
    vec3Row("entity.pos", "Location", e.position, x, y, w, 0.05f);
    vec3Row("entity.rot", "Rotation", e.rotation, x, y, w, 0.5f);
    vec3Row("entity.scale", "Scale", e.scale, x, y, w, 0.01f);
    if (ui::button("Reset rotation", x + labelW, y, (w - labelW - 4) * 0.5f, 22))
        e.rotation[0] = e.rotation[1] = e.rotation[2] = 0.0f;
    if (ui::button("Reset scale", x + labelW + (w - labelW + 4) * 0.5f, y, (w - labelW - 4) * 0.5f, 22))
        e.scale[0] = e.scale[1] = e.scale[2] = 1.0f;
    y += 34;

    sectionLabel("RENDERING", x, y, w);
    y += 22;
    ui::checkbox("Visible", e.visible, x, y);
    ui::checkbox("Collision", e.collision, x + 120, y);
    y += 26;
    if (e.type == LevelEntity::Type::Mesh)
    {
        ui::checkbox("Override color", e.useColor, x, y);
        y += 26;
        if (e.useColor)
        {
            vec3Row("entity.color", "Color", e.color, x, y, w, 0.005f, 0.0f, 1.0f, false);
            ui::text("Opacity", x, y + 4, ui::col::textDim);
            ui::floatField("entity.alpha", e.color[3], x + labelW, y, (w - labelW - 8) / 3.0f, 24, 0.005f, nullptr, 0.0f, 1.0f);
            ui::rect(x + labelW + (w - labelW) / 3.0f + 4, y, (w - labelW) * 2.0f / 3.0f - 4, 24, ui::Color{e.color[0], e.color[1], e.color[2], 1.0f});
            y += 32;
        }
    }

    y += 4;
    sectionLabel("GAMEPLAY", x, y, w);
    y += 22;
    ui::text("Tag", x, y + 4, ui::col::textDim);
    ui::textField("entity.tag", e.tag, x + labelW, y, w - labelW, 24, "group name");
    y += 30;

    ui::text("Properties", x, y, ui::col::textDim);
    y += 22;
    std::string removeKey;
    for (auto &p : e.properties)
    {
        ui::textClipped(p.first, x + 8, y + 4, labelW - 12, ui::col::text);
        ui::textField("entity.prop." + p.first, p.second, x + labelW, y, w - labelW - 30, 24);
        if (ui::button("x", x + w - 26, y, 26, 24))
            removeKey = p.first;
        y += 28;
    }
    if (!removeKey.empty())
        e.properties.erase(removeKey);

    bool add = false;
    if (ui::textField("entity.newprop", newPropertyKey, x, y, labelW + 60, 24, "new key"))
        add = true;
    if (ui::button("Add property", x + labelW + 66, y, w - labelW - 66, 24))
        add = true;
    if (add)
    {
        std::string key = newPropertyKey;
        key.erase(std::remove(key.begin(), key.end(), ' '), key.end());
        if (!key.empty() && !e.properties.count(key))
            e.properties[key] = "";
        newPropertyKey.clear();
    }
    y += 34;
}

void LevelEditor::drawModelPopup()
{
    if (!modelPopupOpen || selected < 0)
    {
        modelPopupOpen = false;
        return;
    }
    LevelEntity &e = scene.level.entities[selected];
    float x = modelPopupAnchor.x, y = modelPopupAnchor.y + 2, w = modelPopupAnchor.w;
    float h = std::min(320.0f, std::max(1, (int)modelAssets.size()) * 22.0f + 8.0f);
    if (y + h > ui::screenHeight() - STATUS_H)
        y = modelPopupAnchor.y - 26 - h - 2;

    ui::beginOverlay(x, y, w, h);
    ui::rect(x - 1, y - 1, w + 2, h + 2, ui::col::accent);
    ui::rect(x, y, w, h, ui::col::panelDark);
    float offset = ui::scrollArea("popup.models", x, y + 4, w, h - 8, modelAssets.size() * 22.0f);
    ui::pushClip(x, y + 4, w - 10, h - 8);
    for (int i = 0; i < (int)modelAssets.size(); i++)
    {
        float iy = y + 4 + i * 22.0f - offset;
        if (ui::listItem(modelAssets[i], x + 2, iy, w - 14, 22, modelAssets[i] == e.model))
        {
            e.model = modelAssets[i];
            scene.refreshModels();
            modelPopupOpen = false;
        }
    }
    if (modelAssets.empty())
        ui::text("No model in Assets", x + 8, y + 6, ui::col::textDim);
    ui::popClip();
    ui::endOverlay();

    // Clic en dehors : fermeture (le champ Modèle gère lui-même son clic)
    if (ui::mousePressed(0) && !(ui::mouseX() >= x && ui::mouseX() < x + w && ui::mouseY() >= y && ui::mouseY() < y + h) &&
        !(ui::mouseX() >= modelPopupAnchor.x && ui::mouseX() < modelPopupAnchor.x + w && ui::mouseY() >= modelPopupAnchor.y - 24 && ui::mouseY() < modelPopupAnchor.y))
        modelPopupOpen = false;
}

void LevelEditor::drawStatusBar()
{
    float w = (float)ui::screenWidth(), y = ui::screenHeight() - STATUS_H;
    ui::rect(0, y, w, STATUS_H, ui::col::panelDark);
    ui::rect(0, y, w, 1, ui::col::background);
    float age = std::chrono::duration<float>(std::chrono::steady_clock::now() - statusTime).count();
    std::string left = (!statusMessage.empty() && age < 4.0f) ? statusMessage
                                                               : "ZQSD move  |  RMB look (+ A/E down/up)  |  Wheel zoom  |  MMB pan  |  W E R gizmo  |  F focus  |  End drop  |  Ctrl S Z Y D C V  |  Del";
    ui::text(left, 10, y + (STATUS_H - ui::lineHeight(ui::Font::Small)) * 0.5f, (!statusMessage.empty() && age < 4.0f) ? ui::col::success : ui::col::textDim, ui::Font::Small);
    std::string right = "Speed " + ui::formatFloat(camSpeed, 1) + "   Pos " + ui::formatFloat(camPos[0], 1) + " " + ui::formatFloat(camPos[1], 1) + " " + ui::formatFloat(camPos[2], 1);
    ui::text(right, w - ui::textWidth(right, ui::Font::Small) - 12, y + (STATUS_H - ui::lineHeight(ui::Font::Small)) * 0.5f, ui::col::textDim, ui::Font::Small);
}

void LevelEditor::drawViewportOverlay()
{
    // Repère des axes en bas à gauche du viewport
    float fwd[3], right[3];
    cameraVectors(fwd, right);
    glm::vec3 f = toVec(fwd), r = toVec(right);
    glm::vec3 up = glm::normalize(glm::cross(r, f));
    float cx = viewport.x + 40, cy = viewport.y + viewport.h - 40;
    ui::rect(cx - 32, cy - 32, 64, 64, ui::Color{0, 0, 0, 0.25f});
    const char *names[3] = {"X", "Y", "Z"};
    for (int a = 0; a < 3; a++)
    {
        glm::vec3 axis(0.0f);
        axis[a] = 1.0f;
        float sx = glm::dot(axis, r) * 24.0f, sy = -glm::dot(axis, up) * 24.0f;
        ui::line(cx, cy, cx + sx, cy + sy, axisColor(a), 2.0f);
        ui::text(names[a], cx + sx * 1.15f - 4, cy + sy * 1.15f - 7, axisColor(a), ui::Font::Small);
    }

    const char *mode = gizmoMode == GizmoMode::Translate ? "Move" : (gizmoMode == GizmoMode::Rotate ? "Rotate" : "Scale");
    std::string info = std::string("Perspective   ") + mode + (snapEnabled ? "   Snap " + ui::formatFloat(gridSize(), 2) : "");
    ui::rect(viewport.x + 8, viewport.y + 8, ui::textWidth(info, ui::Font::Small) + 16, 22, ui::Color{0, 0, 0, 0.35f});
    ui::text(info, viewport.x + 16, viewport.y + 8 + (22 - ui::lineHeight(ui::Font::Small)) * 0.5f, ui::col::text, ui::Font::Small);
}

void LevelEditor::drawModal(Action &action)
{
    if (pending == Pending::None)
        return;
    float sw = (float)ui::screenWidth(), sh = (float)ui::screenHeight();
    ui::beginOverlay(0, 0, sw, sh);
    ui::rect(0, 0, sw, sh, ui::Color{0, 0, 0, 0.5f});
    float w = 440, h = 150, x = (sw - w) * 0.5f, y = (sh - h) * 0.5f;
    ui::rect(x, y, w, h, ui::col::panel);
    ui::rectOutline(x, y, w, h, ui::col::border);
    ui::text("Unsaved changes", x + 20, y + 18, ui::col::text, ui::Font::Large);
    ui::text("Save changes to " + levelRelPath + " ?", x + 20, y + 52, ui::col::textDim);
    float by = y + h - 46;
    if (ui::button("Save", x + w - 330, by, 100, 30, true))
    {
        if (save())
            action = Action::BackToLevels;
        pending = Pending::None;
    }
    if (ui::button("Don't save", x + w - 222, by, 100, 30))
    {
        action = Action::BackToLevels;
        pending = Pending::None;
    }
    if (ui::button("Cancel", x + w - 114, by, 94, 30))
        pending = Pending::None;
    for (unsigned char c : ui::typedChars())
        if (c == 27)
            pending = Pending::None;
    ui::endOverlay();
}
