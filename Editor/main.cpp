#include <windows.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Editor.h"

void display();
void reshape(int, int);
void timer(int);
void globalKeyboard(unsigned char, int, int);
void globalKeyboardUp(unsigned char, int, int);
void globalMouseMotion(int, int);
void updateLoop();
void update();
void setFullscreenBorderless();
void setFullscreen();
void setWindowed(int width, int height);

Editor editor;

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

    editor.init(screenWidth, screenHeight);
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);

    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);

    glutInitWindowSize(screenWidth, screenHeight);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("Engine");

    setWindowed(screenWidth, screenHeight);

    glewInit();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(globalKeyboard);
    glutKeyboardUpFunc(globalKeyboardUp);
    glutPassiveMotionFunc(globalMouseMotion);
    glutMotionFunc(globalMouseMotion);
    glutTimerFunc(0, timer, 0);

    init();

    glutMainLoop();
}

void setFullscreenBorderless()
{
    HWND hwnd = FindWindowA(NULL, "Engine");
    SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_SHOWWINDOW);
}

void setFullscreen()
{
    HWND hwnd = FindWindowA(NULL, "Engine");
    SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    MONITORINFO mi = {sizeof(mi)};
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
    {
        SetWindowPos(hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_SHOWWINDOW);
    }
}

void setWindowed(int width, int height)
{
    HWND hwnd = FindWindowA(NULL, "Engine");
    SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(hwnd, HWND_TOP,
                 (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
                 (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
                 width, height,
                 SWP_SHOWWINDOW);
}

void display()
{
    update();
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    editor.display();

    // === UI START ===
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, screenWidth, 0, screenHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);

    editor.updateUI(screenWidth, screenHeight);

    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    // === UI END ===

    glutSwapBuffers();
}

void update()
{
    editor.update();
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

    gluPerspective(60, aspect, 0.01, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void timer(int)
{
    glutPostRedisplay();
    glutTimerFunc(1000 / 60, timer, 0);
}

void globalKeyboard(unsigned char key, int x, int y)
{
    Editor::globalKeyboard(key, x, y);
}

void globalKeyboardUp(unsigned char key, int x, int y)
{
    Editor::globalKeyboardUp(key, x, y);
}

void globalMouseMotion(int x, int y)
{
    Editor::globalMouseMotion(x, y);
}
