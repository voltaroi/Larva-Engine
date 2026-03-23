#pragma once
#ifndef __LOAD_OPEN_GL__
#define __LOAD_OPEN_GL__
#include <GL/glew.h>
#include <GL/glut.h>
#include <chrono>
#include <iostream>
#include <windows.h>
#include "Game.h"
#include "Engine/Graphics/WindowUtils.h"
#include "Engine/Graphics/ResourcePak.h"

class LoadOpenGl
{
public:
    int main(int argc, char **argv);

    // Static instance used by GLUT callback wrappers
    static LoadOpenGl* instance;

    // GLUT callback wrappers
    static void s_display();
    static void s_reshape(int, int);
    static void s_timer(int);
    static void s_idle();
    static void s_globalKeyboard(unsigned char, int, int);
    static void s_globalKeyboardUp(unsigned char, int, int);
    static void s_globalMouseMotion(int, int);

private:

    Game game;
    WindowUtils windowUtils;

    int screenWidth;
    int screenHeight;

    void init();
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
};

#endif