#include "Spheres.h"
#include <GL/glut.h>
#include <cstdlib>
#include <ctime>
#include <cmath>

#define M_PI 3.14159265358979323846

Spheres::Spheres()
{
    scale.x = 1;
    scale.y = 1;
    scale.z = 1;
}

void Spheres::draw() {
    glPushMatrix();

    glTranslatef(position.x, position.y, position.z);
    glRotatef(rotation.x, 1.0, 0.0, 0.0);
    glRotatef(rotation.y, 0.0, 1.0, 0.0);
    glRotatef(rotation.z, 0.0, 0.0, 1.0);
    glScalef(scale.x, scale.y, scale.z);

    int lats = 20;
    int longs = 20;
    float radius = 1.0;

    for (int i = 0; i < lats; i++) {
        float lat0 = M_PI * (-0.5 + (float)(i) / lats);
        float z0 = sin(lat0);
        float zr0 = cos(lat0);

        float lat1 = M_PI * (-0.5 + (float)(i + 1) / lats);
        float z1 = sin(lat1);
        float zr1 = cos(lat1);

        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= longs; j++) {
            float lng = 2 * M_PI * (float)(j) / longs;
            float x = cos(lng);
            float y = sin(lng);

            glColor3f((rand() % 100) / 100.0f, (rand() % 100) / 100.0f, (rand() % 100) / 100.0f);
            glVertex3f(radius * x * zr0, radius * y * zr0, radius * z0);

            glColor3f((rand() % 100) / 100.0f, (rand() % 100) / 100.0f, (rand() % 100) / 100.0f);
            glVertex3f(radius * x * zr1, radius * y * zr1, radius * z1);
        }
        glEnd();
    }

    glPopMatrix();
}

void Spheres::setRotation(float x, float y, float z) {
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

void Spheres::addRotation(float x, float y, float z) {
    rotation.x += x;
    rotation.y += y;
    rotation.z += z;
}

void Spheres::setPosition(float x, float y, float z)
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

void Spheres::addPosition(float x, float y, float z)
{
    position.x += x;
    position.y += y;
    position.z += z;
}

void Spheres::setScale(float x, float y, float z)
{
    scale.x = x;
    scale.y = y;
    scale.z = z;
}
