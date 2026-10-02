// Larva Engine - éditeur de niveaux
//
//   larva-editor.exe                      écran de choix du projet
//   larva-editor.exe <projet>             liste des niveaux du projet
//   larva-editor.exe <projet> <niveau>    ouvre directement Levels/<niveau>.lvl
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "Browser.h"
#include "EditorUI.h"
#include "LevelEditor.h"
#include "Project.h"

static const char *WINDOW_TITLE = "Larva Editor";

static std::string engineRoot;
static Browser browser;
static LevelEditor editor;
static bool editing = false;
static std::string lastTitle;

// Capture d'écran de contrôle (--screenshot fichier.bmp [--frames N] [--select index])
static std::string screenshotPath;
static int screenshotFrames = 30;
static int frameCount = 0;

static void saveScreenshot(const std::string &path, int w, int h)
{
    std::vector<unsigned char> pixels((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, pixels.data());
    int rowSize = (w * 3 + 3) & ~3;
    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + rowSize * h;
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = h;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f)
        return;
    std::fwrite(&fh, sizeof(fh), 1, f);
    std::fwrite(&ih, sizeof(ih), 1, f);
    std::vector<unsigned char> pad(rowSize - w * 3, 0);
    for (int y = 0; y < h; y++)
    {
        std::fwrite(pixels.data() + (size_t)y * w * 3, 1, w * 3, f);
        if (!pad.empty())
            std::fwrite(pad.data(), 1, pad.size(), f);
    }
    std::fclose(f);
}

static void openLevel(const project::Info &proj, const std::string &levelPath)
{
    std::string message;
    if (editor.open(engineRoot, proj, levelPath, message))
        editing = true;
    else
        browser.setMessage(message, true);
}

static void updateTitle()
{
    std::string title = WINDOW_TITLE;
    if (editing)
        title += std::string(" - ") + browser.selectedProject().name + " - " + editor.levelPath() + (editor.isDirty() ? " *" : "");
    if (title != lastTitle)
    {
        glutSetWindowTitle(title.c_str());
        lastTitle = title;
    }
}

static void display()
{
    int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
    if (w <= 0 || h <= 0)
        return;

    ui::beginFrame(w, h);
    glClearColor(ui::col::background.r, ui::col::background.g, ui::col::background.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (editing)
    {
        if (editor.frame(w, h) == LevelEditor::Action::BackToLevels)
        {
            editor.close();
            editing = false;
            browser.showLevels(browser.selectedProject().name);
        }
    }
    else if (browser.frame(w, h) == Browser::Result::OpenLevel)
        openLevel(browser.selectedProject(), browser.levelToOpen());

    ui::endFrame();
    updateTitle();

    frameCount++;
    if (!screenshotPath.empty() && frameCount == screenshotFrames)
    {
        saveScreenshot(screenshotPath, w, h);
        std::exit(0);
    }
    glutSwapBuffers();
}

static void timer(int)
{
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

static void mouse(int button, int state, int x, int y)
{
    ui::onMouseMove(x, y);
    if (button == 3 || button == 4) // molette (certaines versions de GLUT)
    {
        if (state == GLUT_DOWN)
            ui::onWheel(button == 3 ? 1 : -1);
        return;
    }
    int b = button == GLUT_LEFT_BUTTON ? 0 : (button == GLUT_MIDDLE_BUTTON ? 1 : 2);
    ui::onMouseButton(b, state == GLUT_DOWN);
}

static void motion(int x, int y) { ui::onMouseMove(x, y); }
static void wheel(int, int direction, int x, int y)
{
    ui::onMouseMove(x, y);
    ui::onWheel(direction > 0 ? 1 : -1);
}
static void keyboard(unsigned char key, int, int) { ui::onChar(key); }
static void special(int key, int, int) { ui::onSpecialKey(key); }

static void onClose()
{
    if (editing && editor.isDirty())
    {
        std::string text = "Save changes to " + editor.levelPath() + " before closing?";
        if (MessageBoxA(nullptr, text.c_str(), WINDOW_TITLE, MB_YESNO | MB_ICONQUESTION) == IDYES)
            editor.save();
    }
}

int main(int argc, char **argv)
{
    engineRoot = project::findEngineRoot();
    if (engineRoot.empty())
    {
        MessageBoxA(nullptr, "Cannot find the Larva Engine folder (Engine/ and projects/).\nRun the editor from the engine folder.", WINDOW_TITLE, MB_ICONERROR);
        return 1;
    }

    std::vector<std::string> args;
    int selectIndex = -2;
    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        if (a == "--screenshot" && i + 1 < argc)
            screenshotPath = argv[++i];
        else if (a == "--frames" && i + 1 < argc)
            screenshotFrames = std::atoi(argv[++i]);
        else if (a == "--select" && i + 1 < argc)
            selectIndex = std::atoi(argv[++i]);
        else
            args.push_back(a);
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(1600, 900);
    glutInitWindowPosition(60, 40);
    glutCreateWindow(WINDOW_TITLE);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    if (screenshotPath.empty())
    {
        HWND hwnd = FindWindowA(nullptr, WINDOW_TITLE);
        if (hwnd)
            ShowWindow(hwnd, SW_MAXIMIZE);
    }
    glewInit();

    if (!ui::init("C:/Windows/Fonts/segoeui.ttf") && !ui::init("C:/Windows/Fonts/arial.ttf"))
        std::cerr << "[Editor] Cannot load a font" << std::endl;

    browser.init(engineRoot);
    if (!args.empty() && browser.showLevels(args[0]) && args.size() > 1)
    {
        std::string level = args[1];
        if (level.find('/') == std::string::npos)
            level = "Levels/" + level;
        if (level.size() < 4 || level.substr(level.size() - 4) != ".lvl")
            level += ".lvl";
        openLevel(browser.selectedProject(), level);
        if (editing && selectIndex >= -1)
        {
            editor.selectEntity(selectIndex);
            editor.focusSelection();
        }
    }

    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(motion);
    glutMouseWheelFunc(wheel);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutCloseFunc(onClose);
    glutTimerFunc(16, timer, 0);
    glutMainLoop();
    return 0;
}
