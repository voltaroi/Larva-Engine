#include "Draw2D.h"
#include <GL/glut.h>
#include <cmath>

namespace Draw2D
{
    void setColor(float r, float g, float b, float a)
    {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(r, g, b, a);
    }

    void setLineWidth(float width)
    {
        glLineWidth(width);
    }

    void line(float x0, float y0, float x1, float y1)
    {
        glBegin(GL_LINES);
        glVertex2f(x0, y0);
        glVertex2f(x1, y1);
        glEnd();
    }

    void polyline(const std::vector<float> &points, bool closed)
    {
        glBegin(closed ? GL_LINE_LOOP : GL_LINE_STRIP);
        for (size_t i = 0; i + 1 < points.size(); i += 2)
            glVertex2f(points[i], points[i + 1]);
        glEnd();
    }

    void polygon(const std::vector<float> &points)
    {
        glBegin(GL_TRIANGLE_FAN);
        for (size_t i = 0; i + 1 < points.size(); i += 2)
            glVertex2f(points[i], points[i + 1]);
        glEnd();
    }

    void rect(float x0, float y0, float x1, float y1, bool filled)
    {
        glBegin(filled ? GL_QUADS : GL_LINE_LOOP);
        glVertex2f(x0, y0);
        glVertex2f(x1, y0);
        glVertex2f(x1, y1);
        glVertex2f(x0, y1);
        glEnd();
    }

    void circle(float cx, float cy, float radius, bool filled, int segments)
    {
        glBegin(filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i)
        {
            float a = i * 6.28318531f / segments;
            glVertex2f(cx + std::cos(a) * radius, cy + std::sin(a) * radius);
        }
        glEnd();
    }

    void arc(float cx, float cy, float radius, float angle0, float angle1, int segments)
    {
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= segments; ++i)
        {
            float a = angle0 + (angle1 - angle0) * i / segments;
            glVertex2f(cx + std::cos(a) * radius, cy + std::sin(a) * radius);
        }
        glEnd();
    }

    void triangle(float x0, float y0, float x1, float y1, float x2, float y2)
    {
        glBegin(GL_TRIANGLES);
        glVertex2f(x0, y0);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glEnd();
    }

    void arrow(float cx, float cy, float size, float dx, float dy)
    {
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6f)
            return;
        dx /= len;
        dy /= len;
        // Pointe vers (dx, dy), base perpendiculaire
        triangle(cx + dx * size, cy + dy * size,
                 cx - dx * size - dy * size, cy - dy * size + dx * size,
                 cx - dx * size + dy * size, cy - dy * size - dx * size);
    }
}
