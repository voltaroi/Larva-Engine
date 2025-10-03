#include "Triangles.h"
#include <GL/glut.h>

Triangles::Triangles()
{
    scale.x = 1;
    scale.y = 1;
    scale.z = 1;
}

void Triangles::draw() {
    glPushMatrix();

    glTranslatef(position.x, position.y, position.z);
    glRotatef(rotation.x, 1.0, 0.0, 0.0);
    glRotatef(rotation.y, 0.0, 1.0, 0.0);
    glRotatef(rotation.z, 0.0, 0.0, 1.0);
    glScalef(scale.x, scale.y, scale.z);

    glBegin(GL_TRIANGLES);

    //Front
    glColor3f(1.0, 0.0, 0.0);
    glVertex3f(-1.0, -1.0, 1.0);
    glVertex3f(1.0, -1.0, 1.0);
    glVertex3f(0.0, 1.0, 0.0);

    //Right
    glColor3f(0.0, 1.0, 0.0);
    glVertex3f(1.0, -1.0, 1.0);
    glVertex3f(1.0, -1.0, -1.0);
    glVertex3f(0.0, 1.0, 0.0);

    //Back
    glColor3f(0.0, 0.0, 1.0);
    glVertex3f(1.0, -1.0, -1.0);
    glVertex3f(-1.0, -1.0, -1.0);
    glVertex3f(0.0, 1.0, 0.0);

    //Left
    glColor3f(1.0, 1.0, 0.0);
    glVertex3f(-1.0, -1.0, -1.0);
    glVertex3f(-1.0, -1.0, 1.0);
    glVertex3f(0.0, 1.0, 0.0);

    glEnd();

    //Bottom
    glBegin(GL_QUADS);
    glColor3f(1.0, 0.0, 1.0);
    glVertex3f(-1.0, -1.0, 1.0);
    glVertex3f(1.0, -1.0, 1.0);
    glVertex3f(1.0, -1.0, -1.0);
    glVertex3f(-1.0, -1.0, -1.0);
    glEnd();

    glPopMatrix();
}

void Triangles::setRotation(float x, float y, float z) {
    if (x != -1) {
        rotation.x = x;
    }
    if (y != -1) {
        rotation.y = y;
    }
    if (z != -1) {
        rotation.z = z;
    }
}

void Triangles::addRotation(float x, float y, float z) {
    rotation.x += x;
    rotation.y += y;
    rotation.z += z;
}

void Triangles::setPosition(float x, float y, float z)
{
    if (x != -1) {
        position.x = x;
    }
    if (y != -1) {
        position.y = y;
    }
    if (z != -1) {
        position.z = z;
    }
}

void Triangles::addPosition(float x, float y, float z)
{
    position.x += x;
    position.y += y;
    position.z += z;
}

void Triangles::setScale(float x, float y, float z)
{
    scale.x = x;
    scale.y = y;
    scale.z = z;
}
