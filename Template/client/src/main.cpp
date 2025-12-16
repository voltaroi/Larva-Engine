#include <GL/glew.h>
#include <GL/glut.h>
#include <chrono>
#include <iostream>
#include <windows.h>
#include "Game.h"
#include "Engine/Graphics/WindowUtils.h"
#include "Engine/Graphics/ResourcePak.h"

void display();
void reshape(int, int);
void timer(int);
void idle();
void globalKeyboard(unsigned char, int, int);
void globalKeyboardUp(unsigned char, int, int);
void globalMouseMotion(int, int);
void updateLoop();
void update();
void setFullscreenBorderless();
void setFullscreen();
void setWindowed(int width, int height);

Game game;
WindowUtils windowUtils;

int screenWidth;
int screenHeight;

static void init()
{
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glEnable(GL_DEPTH_TEST);

    // glEnable(GL_LIGHTING);
    // glEnable(GL_LIGHT0);
    // GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 0.0f };
    // glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    glutSetCursor(GLUT_CURSOR_NONE);

    game.init(screenWidth, screenHeight, windowUtils);
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);

    // Get actual monitor dimensions using GetMonitorInfo
    MONITORINFO mi = {sizeof(mi)};
    if (GetMonitorInfo(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi))
    {
        screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
        screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }
    else
    {
        // Fallback to GetSystemMetrics if GetMonitorInfo fails
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
    }

    glutInitWindowSize(screenWidth, screenHeight);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("Engine");

    windowUtils.setFullscreenBorderless(screenWidth, screenHeight);

    glewInit();

    // Initialize resource PAK
    if (!ResourcePak::Initialize("game.pak")) {
        std::cerr << "Warning: game.pak not found. The game may not work correctly." << std::endl;
    }
    ResourcePak::ListFiles(); // Debug: show available resources

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(globalKeyboard);
    glutKeyboardUpFunc(globalKeyboardUp);
    glutPassiveMotionFunc(globalMouseMotion);
    glutMotionFunc(globalMouseMotion);
    glutTimerFunc(0, timer, 0);
    glutIdleFunc(idle);

    init();

    glutMainLoop();
}

void display()
{
    static int frames = 0;
    static auto lastFPSTime = std::chrono::steady_clock::now();

    update();
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    game.display();

    // === UI START ===
    // Get actual viewport dimensions for accurate UI scaling
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int uiWidth = viewport[2];
    int uiHeight = viewport[3];
    
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, uiWidth, 0, uiHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);

    game.updateUI(uiWidth, uiHeight);

    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    // === UI END ===

    glutSwapBuffers();
    // FPS counter (log once per second)
    // frames++;
    // auto now = std::chrono::steady_clock::now();
    // if (now - lastFPSTime >= std::chrono::seconds(1)) {
    //     std::cout << "[FPS] " << frames << " fps" << std::endl;
    //     frames = 0;
    //     lastFPSTime = now;
    // }
}

void update()
{
    game.update();
}

void reshape(int w, int h)
{
    if (h == 0)
    {
        h = 1;
    }
    screenWidth = w;
    screenHeight = h;

    float aspect = (float)w / (float)h;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(windowUtils.getFOV(), aspect, 0.01, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void timer(int)
{
    glutPostRedisplay();
    glutTimerFunc(1000 / 60, timer, 0);
}

void idle()
{
    glutPostRedisplay();
}

void globalKeyboard(unsigned char key, int x, int y)
{
    Game::globalKeyboard(key, x, y);
}

void globalKeyboardUp(unsigned char key, int x, int y)
{
    Game::globalKeyboardUp(key, x, y);
}

void globalMouseMotion(int x, int y)
{
    Game::globalMouseMotion(x, y);
}
