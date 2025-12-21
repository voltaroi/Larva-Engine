#pragma once
#include <iostream>
#include <GL/freeglut.h>

class WindowUtils
{
private:
    int FOV = 75;
    const char* name = "Larva-Engine";
public:
    void setFullscreenBorderless(int screenWidth, int screenHeight);
    void setFullscreen();
    void setWindowed(int width, int height);
    int getFOV();
    void setFOV(int fov);
    void setName(const char* newName);
};