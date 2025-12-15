#include "Game.h"
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <tuple>
#include "Engine/Graphics/Model.h"

static const float NETWORK_CAMERA_HEIGHT = -3.0f;

std::vector<TextBox> textBoxes;

int Game::mouseX = 0;
int Game::mouseY = 0;
bool Game::mousePressed = false;
bool Game::fontLoaded = false;
GLuint texture;
bool escape = false;
bool mouseIsBlocking = false;
bool ignoreNextMouse = false;

bool inputForward = false;
bool inputBackward = false;
bool inputLeftGlobal = false;
bool inputRightGlobal = false;
bool inputJumpGlobal = false;

Quads q{}, qq{}, qqq{}, floorQuad{};

Triangles t{};

Spheres s{};

Sound sound{}, sound2{};

Camera player{};

Model worldModel;

UIButton ButtonQuit;
UIButton ButtonSetFullscreenBorderless;
UIButton ButtonSetFullscreen;
UIButton ButtonSetWindowed;

Client client;

Chat globalChat;

float test = 1;

struct PlayerCube {
    Quads cube;
    float r, g, b;
    int id = -1;
    float curX = 0.0f;
    float curY = 0.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
};

std::vector<PlayerCube> players;
int myPlayerId = -1;
std::mutex playersMutex;
std::vector<std::string> incomingMessages;
std::mutex incomingMessagesMutex;
int nextInputSeq = 1;
std::vector<std::tuple<int,float,float>> pendingInputs;
std::mutex pendingMutex;
float predictedX = 0.0f;
float predictedY = 0.0f;

void Game::init(int screenWidth, int screenHeight, WindowUtils& windowUtil)
{
    if (client.connectToServer("127.0.0.1", 3000)) {
        client.onMessage = [](const std::string &msg) {
            std::lock_guard<std::mutex> lg(incomingMessagesMutex);
            incomingMessages.push_back(msg);
        };
        client.sendMessage("HELLO");
    }
    windowUtils = &windowUtil;
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

    // sound.play("Sounds/didgerido-79546.mp3", 0, 0, 15);
    // sound2.play("Sounds/drums-274805.mp3", 2, 0, 15);

    player.init(screenWidth, screenHeight);

    predictedX = player.getXPosition();
    predictedY = player.getZPosition();
    if (worldModel.loadFromFile("Models/model.fbx")) {
        worldModel.setScale(0.50f, 0.50f, 0.50f);
        worldModel.setPosition(0.0f, -3.5f, 0.0f);
    }
    texture = UI::loadTexture("Images/Basique_Idle_64x64.png", true);

    textBoxes.emplace_back(50, 100, 200, 30);

    // Initialize chat system
    globalChat.init(client);

    // Global light
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    GLfloat lightPos[] = { 0.0f, 10.0f, 10.0f, 1.0f };
    GLfloat lightCol[] = { 0.95f, 0.95f, 0.95f, 1.0f };
    GLfloat ambientCol[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightCol);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientCol);

    ButtonQuit.init(0, 80, UIButton::AnchorH::Center, UIButton::AnchorV::Middle, 10, 10, "Quitter", 0.0f, 0.5f, 0.5f, false, []()
                { std::cout << "Bouton cliqu� !" << std::endl;
                exit(0); });

    ButtonSetFullscreenBorderless.init(0, 30, UIButton::AnchorH::Center, UIButton::AnchorV::Middle, 10, 10, "FullscreenBorderless", 0.0f, 0.5f, 0.5f, false, [this]()
                { 
                    std::cout << "Bouton cliqu� !" << std::endl;
                    windowUtils->setFullscreenBorderless(800, 600);
                });

    ButtonSetFullscreen.init(0, -20, UIButton::AnchorH::Center, UIButton::AnchorV::Middle, 10, 10, "Fullscreen", 0.0f, 0.5f, 0.5f, false, [this]()
                { std::cout << "Bouton cliqu� !" << std::endl;
                    windowUtils->setFullscreen();
                });

    ButtonSetWindowed.init(0, -70, UIButton::AnchorH::Center, UIButton::AnchorV::Middle, 10, 10, "Windowed", 0.0f, 0.5f, 0.5f, false, [this]()
                { 
                    std::cout << "Bouton cliqu� !" << std::endl;
                    windowUtils->setWindowed(800, 600);
                });

    textBoxes[0].onTextChanged = [](const std::string &newText)
    {
        std::cout << "TextBox 0 modifiee : " << newText << std::endl;
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

    worldModel.draw();

    {
        std::lock_guard<std::mutex> lg(playersMutex);
        for (auto &p : players) {
            p.cube.draw();
        }
    }

    player.networkUpdate();
    player.updateView();
}

void Game::update()
{
    std::vector<std::string> msgsToProcess;
    {
        std::lock_guard<std::mutex> lg(incomingMessagesMutex);
        msgsToProcess.swap(incomingMessages);
    }
    for (const auto &msg : msgsToProcess) {
        std::istringstream iss(msg);
        std::string cmd;
        iss >> cmd;

        if (cmd == "ASSIGN") {
            int id, r, g, b;
            iss >> id >> r >> g >> b;
            myPlayerId = id;
            globalChat.setPlayerId(id);
            std::cout << "Assigned id: " << id << " color=" << r << "," << g << "," << b << std::endl;
        }
        else if (cmd == "EXIST" || cmd == "JOIN") {
            int id, r, g, b;
            float px = 0.0f, py = 0.0f;
            iss >> id >> r >> g >> b;
            if (cmd == "EXIST") {
                iss >> px >> py;
            }
            if (id == myPlayerId) continue;

            {
                std::lock_guard<std::mutex> lg(playersMutex);
                bool found = false;
                for (auto &p : players) {
                    if (p.id == id) {
                        p.r = (float)r; p.g = (float)g; p.b = (float)b;
                        p.curX = px; p.curY = py; p.targetX = px; p.targetY = py;
                        p.cube.setColor(r, g, b);
                        p.cube.setPosition(p.curX, -3.5f, p.curY);
                        found = true; break;
                    }
                }
                if (!found) {
                    PlayerCube pc;
                    pc.id = id;
                    pc.r = (float)r; pc.g = (float)g; pc.b = (float)b;
                    pc.curX = px; pc.curY = py; pc.targetX = px; pc.targetY = py;
                    pc.cube.setColor(r, g, b);
                    pc.cube.setScale(0.5f, 1.0f, 0.5f);
                    pc.cube.setPosition(pc.curX, -3.5f, pc.curY);
                    players.push_back(pc);
                }
            }
        }
        else if (cmd == "STATE") {
            int id; float px, py; int lastSeq = 0;
            iss >> id >> px >> py >> lastSeq;
            if (id == myPlayerId) {
                {
                    std::lock_guard<std::mutex> lg(pendingMutex);
                    std::vector<std::tuple<int,float,float>> remaining;
                    for (auto &pr : pendingInputs) {
                        if (std::get<0>(pr) > lastSeq) remaining.push_back(pr);
                    }
                    pendingInputs.swap(remaining);

                    float px0 = px;
                    float py0 = py;
                    for (auto &pr : pendingInputs) {
                        float dx = std::get<1>(pr);
                        float dz = std::get<2>(pr);
                        px0 += dx;
                        py0 += dz;
                    }
                    predictedX = px0;
                    predictedY = py0;
                }

                player.setPosition(predictedX, NETWORK_CAMERA_HEIGHT, predictedY);
            } else {
                std::lock_guard<std::mutex> lg(playersMutex);
                for (auto &p : players) {
                    if (p.id == id) {
                        p.targetX = px; p.targetY = py;
                        break;
                    }
                }
            }
        }
        else if (cmd == "LEAVE") {
            int id; iss >> id;
            {
                std::lock_guard<std::mutex> lg(playersMutex);
                players.erase(std::remove_if(players.begin(), players.end(), [&](const PlayerCube &pc) { return pc.id == id; }), players.end());
            }
        }
    }
    qq.addRotation(1.0, 1.0, 1.0);
    qqq.addRotation(1.0, 1.0, 1.0);
    t.addRotation(0.0, 1.0, 0.0);
    s.addRotation(1.0, 0.0, 0.0);

    player.networkUpdate();

    cubes.clear();
    cubes.push_back(q.getAABB());
    cubes.push_back(qq.getAABB());
    cubes.push_back(qqq.getAABB());
    cubes.push_back(floorQuad.getAABB());

    static std::chrono::steady_clock::time_point lastPlayersUpdate = std::chrono::steady_clock::now();
    auto nowPlayers = std::chrono::steady_clock::now();
    std::chrono::duration<float> dp = nowPlayers - lastPlayersUpdate;
    float dtPlayers = std::max(0.0001f, dp.count());
    lastPlayersUpdate = nowPlayers;

    const float remoteK = 12.0f;
    float alpha = 1.0f - std::exp(-remoteK * dtPlayers);

    {
        std::lock_guard<std::mutex> lg(playersMutex);
        for (auto &p : players) {
            p.curX += (p.targetX - p.curX) * alpha;
            p.curY += (p.targetY - p.curY) * alpha;
            p.cube.setPosition(p.curX, -3.5f, p.curY);
        }
    }

    static std::chrono::steady_clock::time_point lastSend = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (now - lastSend >= std::chrono::milliseconds(50)) {
        lastSend = now;

        if (myPlayerId != -1) {
            int mask = 0;
                    if (inputForward) mask |= 0x01;
                    if (inputBackward) mask |= 0x02;
                    if (inputLeftGlobal) mask |= 0x04;
                    if (inputRightGlobal) mask |= 0x08;
            if (inputJumpGlobal) mask |= 0x10;

            int seq = nextInputSeq++;

            float speed = 0.35f;
            float forwardX = player.getFrontX();
            float forwardZ = player.getFrontZ();
            float rightX = forwardZ;
            float rightZ = -forwardX;

            float dx = 0.0f, dz = 0.0f;
            if (mask & 0x01) { dx += forwardX * speed; dz += forwardZ * speed; }
            if (mask & 0x02) { dx -= forwardX * speed; dz -= forwardZ * speed; }
            if (mask & 0x04) { dx += rightX * speed; dz += rightZ * speed; }
            if (mask & 0x08) { dx -= rightX * speed; dz -= rightZ * speed; }

            {
                std::lock_guard<std::mutex> lg(pendingMutex);
                pendingInputs.emplace_back(seq, dx, dz);
                predictedX += dx;
                predictedY += dz;
            }

            player.setPosition(predictedX, NETWORK_CAMERA_HEIGHT, predictedY);
            // std::cout << "[NET] Sent INP seq=" << seq << " dx=" << dx << " dz=" << dz << " predicted=(" << predictedX << "," << predictedY << ")\n";

            char buf[128];
            std::snprintf(buf, sizeof(buf), "INP %d %f %f\n", seq, dx, dz);
            client.sendMessage(std::string(buf));
        }
    }
}

void Game::updateUI(int screenWidth, int screenHeight)
{
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_NORMALIZE);

    UI::drawText(20, 65, "HP:");
    UI::drawProgressBar(60, 60, 200, 20, 0.75f, 1, 0, 0);

    if (escape)
    {
        UI::drawBox(screenWidth / 2 - 150, screenHeight / 2 - 225, 300, 450, 0.2f, 0.2f, 0.2f, 0.8f, false, 15.0f);
        
        // Load font only once
        if (!fontLoaded) {
            UI::loadfont("Fonts/KiwiSoda.ttf");
            fontLoaded = true;
        }
        
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

    // Draw chat
    globalChat.draw(screenWidth, screenHeight);

    glPopAttrib();
}

void Game::globalKeyboard(unsigned char key, int x, int y)
{
    // Handle chat input if chat is open
    if (globalChat.isVisible()) {
        globalChat.handleKey(key);
        glutPostRedisplay();
        return;
    }

    if (!escape)
    {
        player.keyboard(key, x, y);
    }

    switch (key)
    {
    case 't':
        escape = !escape;
        break;
    case 'c': case 'C':
        globalChat.toggleVisible();
        break;
    }
    switch (key)
    {
    case 'z': case 'Z': inputForward = true; break;
    case 's': case 'S': inputBackward = true; break;
    case 'q': case 'Q': inputLeftGlobal = true; break;
    case 'd': case 'D': inputRightGlobal = true; break;
    case ' ': inputJumpGlobal = true; break;
    }
    for (auto &tb : textBoxes)
        tb.handleKey(key);
    glutPostRedisplay();
}

void Game::globalKeyboardUp(unsigned char key, int x, int y)
{
    player.keyboardUp(key, x, y);

    switch (key)
    {
    case 'z': case 'Z': inputForward = false; break;
    case 's': case 'S': inputBackward = false; break;
    case 'q': case 'Q': inputLeftGlobal = false; break;
    case 'd': case 'D': inputRightGlobal = false; break;
    case ' ': inputJumpGlobal = false; break;
    }
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