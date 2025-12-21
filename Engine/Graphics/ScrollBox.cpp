#include "ScrollBox.h"
#include <algorithm>
#include <iostream>

ScrollBox::ScrollBox() 
    : x(0), y(0), width(200), height(300), 
      r(0.2f), g(0.2f), b(0.2f), alpha(0.9f),
      scrollOffset(0), maxScrollOffset(0), itemSpacing(10.0f), contentHeight(0)
{
}

ScrollBox::~ScrollBox()
{
}

void ScrollBox::init(float x, float y, float width, float height, 
                     float r, float g, float b, float alpha)
{
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
    this->r = r;
    this->g = g;
    this->b = b;
    this->alpha = alpha;
    this->scrollOffset = 0;
    this->itemSpacing = 10.0f;
    updateContentHeight();
}

void ScrollBox::addText(const std::string& text)
{
    items.emplace_back(text);
    updateContentHeight();
}

void ScrollBox::addButton(UIButton* button)
{
    if (button) {
        items.emplace_back(button);
        updateContentHeight();
    }
}

void ScrollBox::clear()
{
    items.clear();
    scrollOffset = 0;
    updateContentHeight();
}

void ScrollBox::updateContentHeight()
{
    contentHeight = 0;
    const float textHeight = 20.0f;
    
    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) {
            contentHeight += itemSpacing;
        }
        
        if (items[i].type == Item::TEXT) {
            contentHeight += textHeight;
        } else if (items[i].type == Item::BUTTON && items[i].button) {
            // Ensure button dimensions are calculated
            if (items[i].button->width <= 0 || items[i].button->height <= 0) {
                items[i].button->updateSizeFromLabel();
            }
            contentHeight += items[i].button->height;
        }
    }
    
    maxScrollOffset = std::max(0.0f, contentHeight - height + 20.0f);
    scrollOffset = std::min(scrollOffset, maxScrollOffset);
}

void ScrollBox::updateItemPositions()
{
    float currentY = y + height - 10.0f + scrollOffset;
    const float textHeight = 20.0f;

    std::cout << currentY << std::endl;
    
    for (auto& item : items) {
        item.offsetY = currentY;
        
        if (item.type == Item::TEXT) {
            currentY -= textHeight;
        } else if (item.type == Item::BUTTON && item.button) {
            // Ensure button has valid dimensions
            if (item.button->width <= 0 || item.button->height <= 0) {
                item.button->updateSizeFromLabel();
            }
            // Update button position
            item.button->x = x + 10.0f;
            item.button->y = currentY - item.button->height;
            currentY -= item.button->height;
        }
        
        currentY -= itemSpacing;
    }
}

bool ScrollBox::isMouseInside(float mouseX, float mouseY)
{
    return mouseX >= x && mouseX <= x + width &&
           mouseY >= y && mouseY <= y + height;
}

void ScrollBox::update(float mouseX, float mouseY, bool mousePressed)
{
    updateItemPositions();
    
    // Manual hit-testing and click handling for buttons in the scrollbox
    for (auto& item : items) {
        if (item.type == Item::BUTTON && item.button) {
            // Check if button is within visible area
            float btnY = item.offsetY - item.button->height;
            float btnX = x + 10.0f;
            float btnW = item.button->width;
            float btnH = item.button->height;
            
            bool visible = (btnY + btnH >= y && btnY <= y + height);
            if (!visible) continue;

            // Hover detection within scrollbox bounds
            bool insideScroll = isMouseInside(mouseX, mouseY);
            bool hovered = insideScroll && (mouseX >= btnX && mouseX <= btnX + btnW &&
                                            mouseY >= btnY && mouseY <= btnY + btnH);
            item.button->hovered = hovered;

            // Click detection on press edge
            if (hovered && mousePressed && !prevMousePressed && item.button->onClick) {
                item.button->onClick();
            }
        }
    }

    prevMousePressed = mousePressed;
}

void ScrollBox::beginClip()
{
    // Convert coordinates to window coordinates
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    int vpX = viewport[0];
    int vpY = viewport[1];
    int vpW = viewport[2];
    int vpH = viewport[3];

    // If viewport is invalid (e.g., minimized), skip clipping and ensure scissor is disabled
    if (vpW <= 0 || vpH <= 0) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    int scissorX = static_cast<int>(x);
    int scissorY = static_cast<int>(y);
    int scissorWidth = static_cast<int>(width);
    int scissorHeight = static_cast<int>(height);

    // Clamp values to viewport
    scissorX = std::max(vpX, std::min(scissorX, vpX + vpW));
    scissorY = std::max(vpY, std::min(scissorY, vpY + vpH));
    int maxW = (vpX + vpW) - scissorX;
    int maxH = (vpY + vpH) - scissorY;
    scissorWidth = std::max(0, std::min(scissorWidth, maxW));
    scissorHeight = std::max(0, std::min(scissorHeight, maxH));

    if (scissorWidth > 0 && scissorHeight > 0) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(scissorX, scissorY, scissorWidth, scissorHeight);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

void ScrollBox::endClip()
{
    glDisable(GL_SCISSOR_TEST);
}

void ScrollBox::draw()
{
    // Draw background box
    UI::drawBox(x, y, width, height, r, g, b, alpha, false, 0.0f);
    
    // Enable clipping
    beginClip();
    
    // Draw items
    updateItemPositions();
    
    for (auto& item : items) {
        if (item.type == Item::TEXT) {
            // Check if text is within visible area
            if (item.offsetY >= y && item.offsetY <= y + height) {
                UI::drawText(x + 10.0f, item.offsetY - 15.0f, item.text.c_str());
            }
        } else if (item.type == Item::BUTTON && item.button) {
            // Check if button is within visible area
            float btnY = item.offsetY - item.button->height;
            if (btnY + item.button->height >= y && btnY <= y + height) {
                float btnX = x + 10.0f;
                float btnWidth = item.button->width;
                float btnHeight = item.button->height;
                
                // Manually draw button
                if (item.button->hovered)
                    UI::drawBox(btnX, btnY, btnWidth, btnHeight, item.button->r, item.button->g, item.button->b, 1.0f, false, 5.0f);
                else
                    UI::drawBox(btnX, btnY, btnWidth, btnHeight, item.button->r + 0.1f, item.button->g + 0.1f, item.button->b + 0.1f, 1.0f, false, 5.0f);

                float textX = btnX + btnWidth / 2 - (glutBitmapLength(GLUT_BITMAP_HELVETICA_18, reinterpret_cast<const unsigned char*>(item.button->label.c_str())) / 2.0f);
                float textY = btnY + btnHeight / 2 - 5;
                UI::drawText(textX, textY, item.button->label.c_str());
            }
        }
    }
    
    // Disable clipping
    endClip();
    
    // Draw scrollbar
    if (contentHeight > height) {
        float scrollbarHeight = (height / contentHeight) * height;
        float scrollbarY = y + height - (scrollOffset / maxScrollOffset) * (height - scrollbarHeight) - scrollbarHeight;
        
        UI::drawBox(x + width - 10.0f, scrollbarY, 8.0f, scrollbarHeight, 
                   0.5f, 0.5f, 0.5f, 0.8f, false, 4.0f);
    }
}

void ScrollBox::scroll(float delta)
{
    scrollOffset -= delta;
    scrollOffset = std::max(0.0f, std::min(scrollOffset, maxScrollOffset));

    std::cout << scrollOffset << std::endl;
}

void ScrollBox::handleMouseWheel(int wheel, int direction, int x, int y)
{
    int winH = glutGet(GLUT_WINDOW_HEIGHT);
    if (winH <= 0) {
        return;
    }
    float mouseX = static_cast<float>(x);
    float mouseY = static_cast<float>(winH - y);
    
    if (isMouseInside(mouseX, mouseY)) {
        float scrollAmount = 20.0f;
        if (direction > 0) {
            scroll(scrollAmount);
        } else {
            scroll(-scrollAmount);
        }
    }
}
