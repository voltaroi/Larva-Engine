#include "Editor.h"

std::vector<TextBox> textBoxes;

int Editor::mouseX = 0;
int Editor::mouseY = 0;
bool Editor::mousePressed = false;
GLuint texture;
bool escape = false;
bool mouseIsBlocking = false;
bool ignoreNextMouse = false;

Quads q{}, qq{}, qqq{}, floorQuad{};

Triangles t{};

Spheres s{};

// Sound sound{}, sound2{};

Camera player{};

UIButton myButton;

float test = 1;

std::vector<std::string> terminalLines;

void Editor::init(int screenWidth, int screenHeight)
{
    escape = true;
    q.setColor(210, 92, 41);
    q.setScale(1, 1, 0.2);

    q.setPosition(0.0, -4.0, 15.0);
    qq.setPosition(2.0, -2.0, 15.0);
    qqq.setPosition(4.0, -2.0, 15.0);
    t.setPosition(0.0, -2.0, 0.0);
    s.setPosition(4.0, -2.0, 0.0);
    floorQuad.setPosition(0.0, -5.0, 0.0);
    floorQuad.setScale(50, 1, 50);

    // sound.setVolumeMulti(0.2);
    // sound2.setVolumeMulti(0.2);

    // sound.setMaxDistance(100);

    // sound.play("Assets/Sounds/didgerido-79546.mp3", 0, 0, 15);
    // sound2.play("Assets/Sounds/drums-274805.mp3", 2, 0, 15);

    player.init(screenWidth, screenHeight);
    texture = UI::loadTexture("Assets/Images/Basique_Idle_64x64.png", true);

    textBoxes.emplace_back(50, 50, 200, 30);
    textBoxes.emplace_back(50, 100, 200, 30);
    myButton.init(10, screenHeight - 100, 150, 35, "Build", 0.0f, 0.5f, 0.5f, false, []()
                  { 
                    // std::cout << "Build start" << std::endl;
                    // system("cd /d ..\\.. && BuildGame.bat");

                    terminalLines.clear();
                    runBuildBatch(terminalLines); });

    textBoxes[0].onTextChanged = [](const std::string &newText)
    {
        std::cout << "TextBox 0 modifi�e : " << newText << std::endl;
    };

    textBoxes[1].onTextChanged = [](const std::string &newText)
    {
        std::cout << "TextBox 1 modifi�e : " << newText << std::endl;
    };
}

void Editor::display()
{
    // sound.updateListenerPosition(player);
    // sound2.updateListenerPosition(player);
    q.draw();
    qq.draw();
    qqq.draw();
    t.draw();
    s.draw();
    floorQuad.draw();

    player.updateView();
}

void Editor::update()
{
    qq.addRotation(1.0, 1.0, 1.0);
    qqq.addRotation(1.0, 1.0, 1.0);
    t.addRotation(0.0, 1.0, 0.0);
    s.addRotation(1.0, 0.0, 0.0);

    cubes.clear();
    cubes.push_back(q.getAABB());
    cubes.push_back(qq.getAABB());
    cubes.push_back(qqq.getAABB());
    cubes.push_back(floorQuad.getAABB());
    player.update(cubes);
}

void Editor::updateUI(int screenWidth, int screenHeight)
{
    // Progress bar
    // UI::drawText(20, 65, "HP:");
    // UI::drawProgressBar(60, 60, 200, 20, 0.75f, 1, 0, 0);

    if (escape)
    {
        // Left box
        UI::drawBox(0, 0, 400, screenHeight, 0.2f, 0.2f, 0.2f, 1.0f, false, 0.0f);

        // Button quit
        myButton.update(mouseX, mouseY, mousePressed);
        myButton.draw();

        int y = 50;
        for (auto &line : terminalLines)
        {
            std::cout << line << std::endl;
            UI::renderText(line, 10, y, 1.0f);
            y += 20;
        }

        // Box
        // UI::drawBox(screenWidth / 2 - 150, screenHeight / 2 - 225, 300, 450, 0.2f, 0.2f, 0.2f, 0.8f, false, 15.0f);

        // Text
        // UI::loadfont("Assets/Fonts/KiwiSoda.ttf");
        // UI::renderText("PAUSE", screenWidth / 2 - 70, screenHeight / 2 + 180, 1.0f);

        // Mouse lock
        glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
        mouseIsBlocking = false;
        glutMouseFunc(Editor::globalMouse);
        glutMotionFunc(Editor::globalMouseMotion);
        glutPassiveMotionFunc(Editor::globalMouseMotion);
    }
    else
    {
        // mouse free
        glutSetCursor(GLUT_CURSOR_NONE);
        blockMouse(screenWidth, screenHeight);
        glutMotionFunc(globalMouseMotion);
        glutPassiveMotionFunc(globalMouseMotion);
    }

    // Image opacity
    if (test <= 0)
    {
        test = 1;
    }
    else
    {
        test -= 0.01;
    }
    UI::drawImage(100, 100, 2, 2, texture, true, test);

    for (const auto &tb : textBoxes)
        tb.draw();
}

void Editor::globalKeyboard(unsigned char key, int x, int y)
{
    if (!escape)
    {
        player.keyboard(key, x, y);
    }

    switch (key)
    {
    case 't':
        escape = !escape;
        break;
    }
    for (auto &tb : textBoxes)
        tb.handleKey(key);
    glutPostRedisplay();
}

void Editor::globalKeyboardUp(unsigned char key, int x, int y)
{
    player.keyboardUp(key, x, y);
}

void Editor::globalMouseMotion(int x, int y)
{
    if (ignoreNextMouse)
    {
        ignoreNextMouse = false;
        return;
    }

    if (!escape)
    {
        player.mouseMotion(x, y);
    }
    mouseX = x;
    mouseY = glutGet(GLUT_WINDOW_HEIGHT) - y;
}

void Editor::globalMouse(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON)
    {
        mousePressed = (state == GLUT_DOWN);
    }
    globalMouseMotion(x, y);

    if (state == GLUT_DOWN)
    {
        float fx = (float)x;
        float fy = glutGet(GLUT_WINDOW_HEIGHT) - y;

        for (auto &tb : textBoxes)
            tb.setFocus(tb.contains(fx, fy));
    }
    glutPostRedisplay();
}

void Editor::blockMouse(int screenWidth, int screenHeight)
{
    if (!mouseIsBlocking)
    {
        ignoreNextMouse = true;
        glutWarpPointer(screenWidth / 2, screenHeight / 2 + 10);
        mouseIsBlocking = true;
    }
}

void Editor::runBuildBatch(std::vector<std::string> &outputLines)
{
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
    {
        std::cerr << "Erreur pipe" << std::endl;
        return;
    }

    PROCESS_INFORMATION pi{};
    STARTUPINFO si{};
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    STARTUPINFOA siA{};

    siA.cb = sizeof(STARTUPINFOA);

    std::string command = "cmd.exe /C BuildGame.bat";

    if (!CreateProcessA(nullptr, command.data(),
                        nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr,
                        &siA, &pi))
    {
        std::cerr << "Impossible de lancer le batch" << std::endl;
        CloseHandle(hWrite);
        CloseHandle(hRead);
        return;
    }

    CloseHandle(hWrite);

    CHAR buffer[256];
    DWORD bytesRead;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
    {
        buffer[bytesRead] = 0;
        std::string s(buffer);

        size_t pos = 0;
        while ((pos = s.find("\r\n")) != std::string::npos)
        {
            outputLines.push_back(s.substr(0, pos));
            s.erase(0, pos + 2);
        }
        if (!s.empty())
            outputLines.push_back(s);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);
}
