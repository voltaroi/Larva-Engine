#pragma once
#include <functional>
#include <string>
#include <GL/glut.h>

class UIButton {
public:
    float x, y, width, height;
    std::string label;
    std::function<void()> onClick;
    bool hovered = false;
    float r, g, b;

    enum class AnchorH { Left, Center, Right };
    enum class AnchorV { Bottom, Middle, Top };

    UIButton();
    void init(float x, float y, AnchorH anchorH = AnchorH::Left, AnchorV anchorV = AnchorV::Top,
              float width = 0.0f, float height = 0.0f, const std::string& label = "", float r = 1.0f, float g = 1.0f, float b = 1.0f, bool border = false, std::function<void()> onClick = nullptr);

    void setAnchor(AnchorH h, AnchorV v);

    void update(float mouseX, float mouseY, bool mousePressed);
    void draw();
    void updateSizeFromLabel();
private:
    bool wasPressedLastFrame = false;
    bool border = false;
    
    float relWidth, relHeight;
    int initScreenWidth, initScreenHeight;

    AnchorH anchorH = AnchorH::Left;
    AnchorV anchorV = AnchorV::Top;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    float extraWidth = 0.0f;
    float extraHeight = 0.0f;
    void* font = GLUT_BITMAP_HELVETICA_18;
    
    void updatePosition();
};