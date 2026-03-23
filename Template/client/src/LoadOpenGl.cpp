#include "LoadOpenGl.h"

// Define static instance
LoadOpenGl* LoadOpenGl::instance = nullptr;

void LoadOpenGl::init()
{
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glEnable(GL_DEPTH_TEST);

    glutSetCursor(GLUT_CURSOR_NONE);

    game.init(screenWidth, screenHeight, windowUtils);
}

int LoadOpenGl::main(int argc, char **argv)
{
    // register this instance for static callbacks
    LoadOpenGl::instance = this;

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
    glutCreateWindow("Larva-Engine");

    windowUtils.setFullscreenBorderless(screenWidth, screenHeight);

    glewInit();

    // Initialize resource PAK
    if (!ResourcePak::Initialize("game.pak")) {
        std::cerr << "Warning: game.pak not found. The game may not work correctly." << std::endl;
    }
    ResourcePak::ListFiles(); // Debug: show available resources

    // Register static wrappers as GLUT callbacks
    glutDisplayFunc(LoadOpenGl::s_display);
    glutReshapeFunc(LoadOpenGl::s_reshape);
    glutKeyboardFunc(LoadOpenGl::s_globalKeyboard);
    glutKeyboardUpFunc(LoadOpenGl::s_globalKeyboardUp);
    glutPassiveMotionFunc(LoadOpenGl::s_globalMouseMotion);
    glutMotionFunc(LoadOpenGl::s_globalMouseMotion);
    glutTimerFunc(0, LoadOpenGl::s_timer, 0);
    glutIdleFunc(LoadOpenGl::s_idle);

    init();

    glutMainLoop();

    return 0;
}

// Static wrappers forward to the instance
void LoadOpenGl::s_display() { if (LoadOpenGl::instance) LoadOpenGl::instance->display(); }
void LoadOpenGl::s_reshape(int w, int h) { if (LoadOpenGl::instance) LoadOpenGl::instance->reshape(w, h); }
void LoadOpenGl::s_timer(int v) { if (LoadOpenGl::instance) LoadOpenGl::instance->timer(v); }
void LoadOpenGl::s_idle() { if (LoadOpenGl::instance) LoadOpenGl::instance->idle(); }
void LoadOpenGl::s_globalKeyboard(unsigned char key, int x, int y) { if (LoadOpenGl::instance) LoadOpenGl::instance->globalKeyboard(key, x, y); }
void LoadOpenGl::s_globalKeyboardUp(unsigned char key, int x, int y) { if (LoadOpenGl::instance) LoadOpenGl::instance->globalKeyboardUp(key, x, y); }
void LoadOpenGl::s_globalMouseMotion(int x, int y) { if (LoadOpenGl::instance) LoadOpenGl::instance->globalMouseMotion(x, y); }

void LoadOpenGl::display()
{
    static int frames = 0;
    static auto lastFPSTime = std::chrono::steady_clock::now();

    update();
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    game.display();

    // === UI START ===
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int uiWidth = viewport[2];
    int uiHeight = viewport[3];

    if (uiWidth > 0 && uiHeight > 0) {
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
    }
    // === UI END ===

    glutSwapBuffers();
}

void LoadOpenGl::update()
{
    game.update();
}

void LoadOpenGl::reshape(int w, int h)
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

void LoadOpenGl::timer(int)
{
    glutPostRedisplay();
    glutTimerFunc(1000 / 60, LoadOpenGl::s_timer, 0);
}

void LoadOpenGl::idle()
{
    glutPostRedisplay();
}

void LoadOpenGl::globalKeyboard(unsigned char key, int x, int y)
{
    Game::globalKeyboard(key, x, y);
}

void LoadOpenGl::globalKeyboardUp(unsigned char key, int x, int y)
{
    Game::globalKeyboardUp(key, x, y);
}

void LoadOpenGl::globalMouseMotion(int x, int y)
{
    Game::globalMouseMotion(x, y);
}
