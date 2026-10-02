#pragma once
#ifndef __LEVEL_EDITOR__
#define __LEVEL_EDITOR__
#include <chrono>
#include <string>
#include <vector>
#include "Project.h"
#include "Engine/Scene/LevelScene.h"

// Écran d'édition d'un niveau : viewport 3D, contenu, outliner, détails, barre d'outils
class LevelEditor
{
public:
    enum class Action
    {
        None,
        BackToLevels
    };

    bool open(const std::string &root, const project::Info &project, const std::string &levelPath, std::string &message);
    void close();
    Action frame(int screenWidth, int screenHeight);

    bool isOpen() const { return opened; }
    bool isDirty() const;
    bool save();
    const std::string &levelPath() const { return levelRelPath; }

    // Caméra placée sur la sélection (ou le niveau)
    void focusSelection();
    void selectEntity(int index) { select(index); }

private:
    enum class GizmoMode
    {
        Translate,
        Rotate,
        Scale
    };

    // Poignées du gizmo : 0..2 axes X Y Z, 3..5 plans (normale X, Y, Z), 6 échelle uniforme
    enum
    {
        HANDLE_NONE = -1,
        HANDLE_PLANE_X = 3,
        HANDLE_UNIFORM = 6
    };

    struct Snapshot
    {
        std::string data;
        int selected;
    };

    struct Rect
    {
        float x, y, w, h;
    };

    // --- Projet / niveau ---
    bool opened = false;
    std::string rootDir;
    project::Info proj;
    std::string levelRelPath;
    LevelScene scene;
    std::vector<std::string> modelAssets;
    int selected = -1;

    // --- Historique ---
    std::vector<Snapshot> undoStack, redoStack;
    std::string committedState;
    std::string savedState;

    // --- Caméra ---
    float camPos[3] = {0.0f, 6.0f, -14.0f};
    float camYaw = 0.0f;   // radians, 0 = regarde vers +Z
    float camPitch = -0.35f;
    float camSpeed = 10.0f;
    bool flying = false;
    bool panning = false;
    int flyAnchorX = 0, flyAnchorY = 0;
    int lastMouseX = 0, lastMouseY = 0;
    std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();
    float frameDt = 0.016f;

    // Matrices du dernier rendu (picking)
    double viewMatrix[16] = {};
    double projMatrix[16] = {};
    int glViewportRect[4] = {0, 0, 1, 1};

    // --- Gizmo ---
    GizmoMode gizmoMode = GizmoMode::Translate;
    bool snapEnabled = true;
    int gridIndex = 2;
    int hoveredHandle = HANDLE_NONE;
    int dragHandle = HANDLE_NONE;
    float dragStartParam = 0.0f;
    float dragStartHit[3] = {0, 0, 0};
    float dragStartPos[3] = {0, 0, 0};
    float dragStartRot[3] = {0, 0, 0};
    float dragStartScale[3] = {1, 1, 1};
    float dragStartLength = 1.0f;
    int dragStartMouseX = 0, dragStartMouseY = 0;
    bool viewportPressed = false;
    int pressMouseX = 0, pressMouseY = 0;

    // --- Interface ---
    Rect viewport{0, 0, 1, 1};
    std::string outlinerFilter;
    float detailsContentHeight = 0.0f;
    std::string newPropertyKey;
    bool modelPopupOpen = false;
    Rect modelPopupAnchor{0, 0, 0, 0};
    int contentDragIndex = -2; // -2 aucun, -1 PlayerStart, -3 Marker, >= 0 modèle
    bool contentDragging = false;
    unsigned long lastOutlinerClickTime = 0;
    int lastOutlinerClickIndex = -1;
    std::string statusMessage;
    std::chrono::steady_clock::time_point statusTime;
    bool hasClipboard = false;
    LevelEntity clipboard;

    enum class Pending
    {
        None,
        LeaveConfirm
    };
    Pending pending = Pending::None;

    // Cadre
    void layout(int w, int h);
    void handleShortcuts(Action &action);
    void updateCamera();
    void render3D();
    void viewportInput();

    // Panneaux
    void drawToolbar(Action &action);
    void drawContent();
    void drawOutliner(float x, float y, float w, float h);
    void drawDetails(float x, float y, float w, float h);
    void drawLevelSettings(float x, float &y, float w);
    void drawEntityDetails(LevelEntity &e, float x, float &y, float w);
    void drawStatusBar();
    void drawModelPopup();
    void drawContentDrag();
    void drawModal(Action &action);
    void drawViewportOverlay();

    // Gizmo
    float gizmoLength() const;
    bool project(const float p[3], float &sx, float &sy) const;
    void mouseRay(int mx, int my, float origin[3], float dir[3]) const;
    int pickHandle(int mx, int my) const;
    void beginGizmoDrag(int handle, int mx, int my);
    void updateGizmoDrag(int mx, int my);
    bool axisParam(int axis, const float origin[3], const float dir[3], float &param) const;
    bool planeHit(int normalAxis, const float origin[3], const float dir[3], float hit[3]) const;
    void drawGizmo();
    void drawGrid();
    void drawHelpers();

    // Édition
    float gridSize() const;
    float snap(float v, float step) const;
    void select(int index);
    int addEntity(const LevelEntity &e);
    void deleteSelected();
    void duplicateSelected(bool offset);
    void dropToFloor();
    LevelEntity makeEntity(int contentIndex) const;
    void placementPoint(int mx, int my, float out[3]) const;
    void spawnInFront(int contentIndex);

    // Historique
    void checkpoint();
    void undo();
    void redo();
    void restore(const Snapshot &s);

    void setStatus(const std::string &message);
    void cameraVectors(float forward[3], float right[3]) const;
};

#endif
