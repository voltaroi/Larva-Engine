#include <GL/glew.h>
#include <GL/glut.h>
#include "Game.h"
#include "Engine/Graphics/WindowUtils.h"

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

    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);

    glutInitWindowSize(screenWidth, screenHeight);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("Engine");

    windowUtils.setFullscreenBorderless(screenWidth, screenHeight);

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

void display()
{
    update();
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    game.display();

    // === UI START ===
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, screenWidth, 0, screenHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);

    game.updateUI(screenWidth, screenHeight);

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
