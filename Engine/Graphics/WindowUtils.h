#pragma once

class WindowUtils
{
private:
    int FOV = 75;
public:
    void setFullscreenBorderless(int screenWidth, int screenHeight);
    void setFullscreen();
    void setWindowed(int width, int height);
    int getFOV();
    void setFOV(int fov);
};