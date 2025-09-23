#pragma once
#include <string>
#include <GL/glut.h>
#include <functional>

class TextBox {
public:
    float x, y, width, height;
    bool focused;
    std::string text;

    std::function<void(const std::string&)> onTextChanged;

    TextBox(float x, float y, float width, float height);

    void draw() const;
    void handleKey(unsigned char key);
    bool contains(float mx, float my) const;
    void setFocus(bool f);
};
