#include "Game.h"

std::vector<TextBox> textBoxes;

int Game::mouseX = 0;
int Game::mouseY = 0;
bool Game::mousePressed = false;
GLuint texture;
bool escape = false;
bool mouseIsBlocking = false;
bool ignoreNextMouse = false;

Quads q{}, qq{}, qqq{}, floorQuad{};

Triangles t{};

Spheres s{};

Sound sound{}, sound2{};

Camera player{};

UIButton ButtonQuit;
UIButton ButtonSetFullscreenBorderless;
UIButton ButtonSetFullscreen;
UIButton ButtonSetWindowed;

Client client;

float test = 1;

struct PlayerCube {
    Quads cube;
    float r, g, b;
};

std::vector<PlayerCube> players;

void Game::init(int screenWidth, int screenHeight, WindowUtils& windowUtil)
{
    if (client.connectToServer("127.0.0.1", 3000)) {
        client.sendMessage("Hello World!");
    }
    windowUtils = &windowUtil;
    // windowUtils->setFOV(120);
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

    ButtonQuit.init(screenWidth / 2 - 75, screenHeight / 2 + 80, 150, 40, "Quitter", 0.0f, 0.5f, 0.5f, false, []()
                { std::cout << "Bouton cliqu� !" << std::endl;
                exit(0); });

    ButtonSetFullscreenBorderless.init(screenWidth / 2 - 100, screenHeight / 2 + 40 -10, 200, 40, "FullscreenBorderless", 0.0f, 0.5f, 0.5f, false, [this]()
                { 
                    std::cout << "Bouton cliqu� !" << std::endl;
                    windowUtils->setFullscreenBorderless(800, 600);
                });

    ButtonSetFullscreen.init(screenWidth / 2 - 75, screenHeight / 2 + 0-20, 150, 40, "Fullscreen", 0.0f, 0.5f, 0.5f, false, [this]()
                { std::cout << "Bouton cliqu� !" << std::endl;
                    windowUtils->setFullscreen();
                });

    ButtonSetWindowed.init(screenWidth / 2 - 75, screenHeight / 2 + -40-30, 150, 40, "Windowed", 0.0f, 0.5f, 0.5f, false, [this]()
                { 
                    std::cout << "Bouton cliqu� !" << std::endl;
                    windowUtils->setWindowed(800, 600);
                });

    textBoxes[0].onTextChanged = [](const std::string &newText)
    {
        std::cout << "TextBox 0 modifi�e : " << newText << std::endl;
    };

    textBoxes[1].onTextChanged = [](const std::string &newText)
    {
        std::cout << "TextBox 1 modifi�e : " << newText << std::endl;
    };
}

void Game::display()
{
    // sound.updateListenerPosition(player);
    // sound2.updateListenerPosition(player);
    q.draw();
    qq.draw();
    qqq.draw();
    t.draw();
    s.draw();
    floorQuad.draw();

    for (auto &p : players) {
        p.cube.draw();
    }

    player.updateView();
}

void Game::update()
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

void Game::updateUI(int screenWidth, int screenHeight)
{
    UI::drawText(20, 65, "HP:");
    UI::drawProgressBar(60, 60, 200, 20, 0.75f, 1, 0, 0);

    if (escape)
    {
        UI::drawBox(screenWidth / 2 - 150, screenHeight / 2 - 225, 300, 450, 0.2f, 0.2f, 0.2f, 0.8f, false, 15.0f);
        UI::loadfont("Assets/Fonts/KiwiSoda.ttf");
        UI::renderText("PAUSE", screenWidth / 2 - 70, screenHeight / 2 + 180, 1.0f);

        ButtonQuit.update(mouseX, mouseY, mousePressed);
        ButtonQuit.draw();

        ButtonSetFullscreenBorderless.update(mouseX, mouseY, mousePressed);
        ButtonSetFullscreenBorderless.draw();
        ButtonSetFullscreen.update(mouseX, mouseY, mousePressed);
        ButtonSetFullscreen.draw();
        ButtonSetWindowed.update(mouseX, mouseY, mousePressed);
        ButtonSetWindowed.draw();

        glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
        mouseIsBlocking = false;
        glutMouseFunc(Game::globalMouse);
        glutMotionFunc(Game::globalMouseMotion);
        glutPassiveMotionFunc(Game::globalMouseMotion);
    }
    else
    {
        glutSetCursor(GLUT_CURSOR_NONE);
        blockMouse(screenWidth, screenHeight);
        glutMotionFunc(globalMouseMotion);
        glutPassiveMotionFunc(globalMouseMotion);
    }

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

void Game::globalKeyboard(unsigned char key, int x, int y)
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

void Game::globalKeyboardUp(unsigned char key, int x, int y)
{
    player.keyboardUp(key, x, y);
}

void Game::globalMouseMotion(int x, int y)
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

void Game::globalMouse(int button, int state, int x, int y)
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

void Game::blockMouse(int screenWidth, int screenHeight)
{
    if (!mouseIsBlocking)
    {
        ignoreNextMouse = true;
        glutWarpPointer(screenWidth / 2, screenHeight / 2 + 10);
        mouseIsBlocking = true;
    }
}