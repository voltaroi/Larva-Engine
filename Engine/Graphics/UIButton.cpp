#include "UIButton.h"
#include "UI.h"

UIButton::UIButton()
{
    manualPosition = false;
}

void UIButton::init(float x, float y, AnchorH anchorH, AnchorV anchorV,
                    float width, float height, const std::string &label,
                    float r, float g, float b, bool border, float radius, std::function<void()> onClick)
{
    // Store initial screen dimensions
    initScreenWidth = glutGet(GLUT_WINDOW_WIDTH);
    initScreenHeight = glutGet(GLUT_WINDOW_HEIGHT);

    // Offsets are now explicit pixel offsets from the chosen anchors
    offsetX = x;
    offsetY = y;

    // Store extra size
    extraWidth = width;
    extraHeight = height;

    relWidth = 0.0f;
    relHeight = 0.0f;

    // Set initial absolute positions
    this->x = x;
    this->y = y;
    this->width = 0.0f;
    this->height = 0.0f;

    this->label = label;
    this->r = r;
    this->g = g;
    this->b = b;
    this->border = border;
    this->radius = radius;
    this->onClick = onClick;
    this->anchorH = anchorH;
    this->anchorV = anchorV;

    updateSizeFromLabel();
}

void UIButton::setAnchor(AnchorH h, AnchorV v)
{
    anchorH = h;
    anchorV = v;
}

void UIButton::updatePosition()
{
    int currentWidth = glutGet(GLUT_WINDOW_WIDTH);
    int currentHeight = glutGet(GLUT_WINDOW_HEIGHT);

    width = relWidth * currentWidth;
    height = relHeight * currentHeight;

    float baseX = 0.0f;
    float baseY = 0.0f;

    switch (anchorH)
    {
    case AnchorH::Left:
        baseX = 0.0f;
        break;
    case AnchorH::Center:
        baseX = (currentWidth - width) * 0.5f;
        break;
    case AnchorH::Right:
        baseX = currentWidth - width;
        break;
    }

    switch (anchorV)
    {
    case AnchorV::Bottom:
        baseY = 0.0f;
        break;
    case AnchorV::Middle:
        baseY = (currentHeight - height) * 0.5f;
        break;
    case AnchorV::Top:
        baseY = currentHeight - height;
        break;
    }

    x = baseX + offsetX;
    y = baseY + offsetY;
}

void UIButton::updateSizeFromLabel()
{
    if (!font)
        font = GLUT_BITMAP_HELVETICA_18;

    const unsigned char *ulabel = reinterpret_cast<const unsigned char *>(label.c_str());
    int textWidth = glutBitmapLength(font, ulabel);
    const float textHeight = 18.0f;

    float desiredWidth = static_cast<float>(textWidth) + extraWidth;
    float desiredHeight = textHeight + extraHeight;

    int currentWidth = glutGet(GLUT_WINDOW_WIDTH);
    int currentHeight = glutGet(GLUT_WINDOW_HEIGHT);

    if (currentWidth > 0)
        relWidth = desiredWidth / static_cast<float>(currentWidth);
    if (currentHeight > 0)
        relHeight = desiredHeight / static_cast<float>(currentHeight);

    width = desiredWidth;
    height = desiredHeight;
}

void UIButton::update(float mouseX, float mouseY, bool mousePressed)
{
    if (!manualPosition) {
        updateSizeFromLabel();
        updatePosition();
    }

    hovered = mouseX >= x && mouseX <= x + width &&
              mouseY >= y && mouseY <= y + height;

    if (hovered && mousePressed && !wasPressedLastFrame && onClick)
    {
        onClick();
    }

    wasPressedLastFrame = mousePressed;
}

void UIButton::draw()
{
    if (!manualPosition) {
        updateSizeFromLabel();
        updatePosition();
    }

    if (hovered)
        UI::drawBox(x, y, width, height, r, g, b, 1.0f, border, radius);
    else
        UI::drawBox(x, y, width, height, r + 0.1f, g + 0.1f, b + 0.1f, 1.0f, border, radius);

    float textX = x + width / 2 - (glutBitmapLength(font, reinterpret_cast<const unsigned char *>(label.c_str())) / 2.0f);
    float textY = y + height / 2 - 5;
    UI::drawText(textX, textY, label.c_str());
}

void UIButton::setPosition(float x, float y, float width, float height)
{
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
}