#pragma once
#include <functional>
#include <string>

class UIButton {
public:
    float x, y, width, height;
    std::string label;
    std::function<void()> onClick;
    bool hovered = false;
    float r, g, b;

    UIButton();
    void init(float x, float y, float width, float height, const std::string& label, float r, float g, float b, bool border, std::function<void()> onClick);

    void update(float mouseX, float mouseY, bool mousePressed);
    void draw();
private:
    bool wasPressedLastFrame = false;
    bool border = false;
};