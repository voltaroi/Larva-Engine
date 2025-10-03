#include "Quads.h"
#include <GL/glut.h>

Quads::Quads()
{
    scale.x = 1;
    scale.y = 1;
    scale.z = 1;
}

void Quads::draw()
{
    glPushMatrix();

    glTranslatef(position.x, position.y, position.z);
    glRotatef(rotation.x, 1.0, 0.0, 0.0);
    glRotatef(rotation.y, 0.0, 1.0, 0.0);
    glRotatef(rotation.z, 0.0, 0.0, 1.0);
    glScalef(scale.x, scale.y, scale.z);

    glBegin(GL_QUADS);

    if (HasColor)
    {
        // front
        glColor3f(color.x, color.y, color.z);
        glVertex3f(-1.0, 1.0, 1.0);
        glVertex3f(-1.0, -1.0, 1.0);
        glVertex3f(1.0, -1.0, 1.0);
        glVertex3f(1.0, 1.0, 1.0);
        // back
        glColor3f(color.x, color.y, color.z);
        glVertex3f(1.0, 1.0, -1.0);
        glVertex3f(1.0, -1.0, -1.0);
        glVertex3f(-1.0, -1.0, -1.0);
        glVertex3f(-1.0, 1.0, -1.0);
        // right
        glColor3f(color.x, color.y, color.z);
        glVertex3f(1.0, 1.0, 1.0);
        glVertex3f(1.0, -1.0, 1.0);
        glVertex3f(1.0, -1.0, -1.0);
        glVertex3f(1.0, 1.0, -1.0);
        // left
        glColor3f(color.x, color.y, color.z);
        glVertex3f(-1.0, 1.0, -1.0);
        glVertex3f(-1.0, -1.0, -1.0);
        glVertex3f(-1.0, -1.0, 1.0);
        glVertex3f(-1.0, 1.0, 1.0);
        // top
        glColor3f(color.x, color.y, color.z);
        glVertex3f(-1.0, 1.0, -1.0);
        glVertex3f(-1.0, 1.0, 1.0);
        glVertex3f(1.0, 1.0, 1.0);
        glVertex3f(1.0, 1.0, -1.0);
        // bottom
        glColor3f(color.x, color.y, color.z);
        glVertex3f(-1.0, -1.0, -1.0);
        glVertex3f(-1.0, -1.0, 1.0);
        glVertex3f(1.0, -1.0, 1.0);
        glVertex3f(1.0, -1.0, -1.0);
    }
    else
    {
        // front
        glColor3f(1.0, 0.0, 0.0);
        glVertex3f(-1.0, 1.0, 1.0);
        glVertex3f(-1.0, -1.0, 1.0);
        glVertex3f(1.0, -1.0, 1.0);
        glVertex3f(1.0, 1.0, 1.0);
        // back
        glColor3f(0.0, 1.0, 0.0);
        glVertex3f(1.0, 1.0, -1.0);
        glVertex3f(1.0, -1.0, -1.0);
        glVertex3f(-1.0, -1.0, -1.0);
        glVertex3f(-1.0, 1.0, -1.0);
        // right
        glColor3f(0.0, 0.0, 1.0);
        glVertex3f(1.0, 1.0, 1.0);
        glVertex3f(1.0, -1.0, 1.0);
        glVertex3f(1.0, -1.0, -1.0);
        glVertex3f(1.0, 1.0, -1.0);
        // left
        glColor3f(1.0, 1.0, 0.0);
        glVertex3f(-1.0, 1.0, -1.0);
        glVertex3f(-1.0, -1.0, -1.0);
        glVertex3f(-1.0, -1.0, 1.0);
        glVertex3f(-1.0, 1.0, 1.0);
        // top
        glColor3f(0.0, 1.0, 1.0);
        glVertex3f(-1.0, 1.0, -1.0);
        glVertex3f(-1.0, 1.0, 1.0);
        glVertex3f(1.0, 1.0, 1.0);
        glVertex3f(1.0, 1.0, -1.0);
        // bottom
        glColor3f(1.0, 0.0, 1.0);
        glVertex3f(-1.0, -1.0, -1.0);
        glVertex3f(-1.0, -1.0, 1.0);
        glVertex3f(1.0, -1.0, 1.0);
        glVertex3f(1.0, -1.0, -1.0);
    }
    glEnd();

    glPopMatrix();
}

void Quads::setRotation(float x, float y, float z)
{
    if (x != -1)
    {
        rotation.x = x;
    }
    if (y != -1)
    {
        rotation.y = y;
    }
    if (z != -1)
    {
        rotation.z = z;
    }
}

void Quads::addRotation(float x, float y, float z)
{
    rotation.x += x;
    rotation.y += y;
    rotation.z += z;
}

Vec3 Quads::getRotation()
{
    return rotation;
}

void Quads::setPosition(float x, float y, float z)
{
    if (x != -1)
    {
        position.x = x;
    }
    if (y != -1)
    {
        position.y = y;
    }
    if (z != -1)
    {
        position.z = z;
    }
}

void Quads::addPosition(float x, float y, float z)
{
    position.x += x;
    position.y += y;
    position.z += z;
}

Vec3 Quads::getPosition()
{
    return position;
}

void Quads::setScale(float x, float y, float z)
{
    scale.x = x;
    scale.y = y;
    scale.z = z;
}

void Quads::setColor(float r, float g, float b)
{
    HasColor = true;
    color.x = r / 255;
    color.y = g / 255;
    color.z = b / 255;
}

AABB Quads::getAABB() const
{
    AABB result;

    float halfWidth  = scale.x;
    float halfHeight = scale.y;
    float halfDepth  = scale.z;

    result.minX = position.x - halfWidth;
    result.maxX = position.x + halfWidth;
    result.minY = position.y - halfHeight;
    result.maxY = position.y + halfHeight;
    result.minZ = position.z - halfDepth;
    result.maxZ = position.z + halfDepth;

    return result;
}

void Quads::setName(const char *newName)
{
    name = newName;
}

const char *Quads::getName()
{
    return name;
}
