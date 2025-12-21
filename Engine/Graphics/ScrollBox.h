#pragma once
#ifndef __SCROLLBOX__
#define __SCROLLBOX__

#include <functional>
#include <vector>
#include <string>
#include <GL/glut.h>
#include "UIButton.h"
#include "UI.h"

class ScrollBox {
public:
    struct Item {
        enum Type { TEXT, BUTTON };
        Type type;
        std::string text;
        UIButton* button;
        float offsetY;

        Item(const std::string& text) : type(TEXT), text(text), button(nullptr), offsetY(0.0f) {}
        Item(UIButton* btn) : type(BUTTON), button(btn), offsetY(0.0f) {}
    };

    ScrollBox();
    ~ScrollBox();

    void init(float x, float y, float width, float height, 
              float r = 0.2f, float g = 0.2f, float b = 0.2f, float alpha = 0.9f);
    
    void addText(const std::string& text);
    void addButton(UIButton* button);
    void clear();
    
    void update(float mouseX, float mouseY, bool mousePressed);
    void draw();
    
    void scroll(float delta);
    void handleMouseWheel(int wheel, int direction, int x, int y);

private:
    float x, y, width, height;
    float r, g, b, alpha;
    float scrollOffset;
    float maxScrollOffset;
    float itemSpacing;
    float contentHeight;
    bool prevMousePressed = false;
    
    std::vector<Item> items;
    
    void updateContentHeight();
    void updateItemPositions();
    bool isMouseInside(float mouseX, float mouseY);
    
    // Scissor test for clipping
    void beginClip();
    void endClip();
};

#endif
