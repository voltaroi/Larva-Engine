#include "UIButton.h"
#include "UI.h"

UIButton::UIButton() {
}

void UIButton::init(float x, float y, float width, float height, const std::string& label,
                    float r, float g, float b, bool border, std::function<void()> onClick) {
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
    this->label = label;
    this->r = r;
    this->g = g;
    this->b = b;
    this->border = border;
    this->onClick = onClick;
}

void UIButton::update(float mouseX, float mouseY, bool mousePressed)
{
    hovered = mouseX >= x && mouseX <= x + width &&
        mouseY >= y && mouseY <= y + height;

    if (hovered && mousePressed && !wasPressedLastFrame && onClick) {
        onClick();
    }

    wasPressedLastFrame = mousePressed;
}

void UIButton::draw() {
    if (hovered)
        UI::drawBox(x, y, width, height, r, g, b, 1.0f, border, 5.0f);
    else
        UI::drawBox(x, y, width, height, r+0.1f, g+0.1f, b+0.1f, 1.0f, border, 5.0f);

    float textX = x + width / 2 - (label.length() * 9) / 2;
    float textY = y + height / 2 - 5;
    UI::drawText(textX, textY, label.c_str());
}
