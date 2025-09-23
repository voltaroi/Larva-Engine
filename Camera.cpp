#include "Camera.h"

float cameraX = 0.0f, cameraY = 0.0f, cameraZ = 5.0f;
float cameraAngleX = 0.0f, cameraAngleY = 0.0f;
float moveSpeed = 0.05f;
float rotationSpeed = 0.001f;
float jumpSpeed = 0.15f;
float gravity = 0.006f;
bool isJumping = false;
float verticalVelocity = 0.0f;

int lastX, lastY;

bool moveForward = false, moveBackward = false, moveLeft = false, moveRight = false;

void Camera::init(int w, int h)
{
    screenWidth = w;
    screenHeight = h;

    lastX = screenHeight / 2;
    lastY = screenWidth / 2;
}

void Camera::update(const std::vector<AABB> &collisionBoxes)
{

    float nextX = cameraX;
    float nextZ = cameraZ;

    if (moveForward)
    {
        nextX += moveSpeed * sin(cameraAngleY);
        nextZ += moveSpeed * cos(cameraAngleY);
    }
    if (moveBackward)
    {
        nextX -= moveSpeed * sin(cameraAngleY);
        nextZ -= moveSpeed * cos(cameraAngleY);
    }
    if (moveLeft)
    {
        nextX += moveSpeed * cos(cameraAngleY);
        nextZ -= moveSpeed * sin(cameraAngleY);
    }
    if (moveRight)
    {
        nextX -= moveSpeed * cos(cameraAngleY);
        nextZ += moveSpeed * sin(cameraAngleY);
    }

    if (isJumping || cameraY > 0.0f)
    {
        cameraY += verticalVelocity;
        verticalVelocity -= gravity;
    }

    // --- Collision horizontale ---

    float torsoY = cameraY + playerHeight * 0.25f;

    bool blockedX = false;
    for (const auto &box : collisionBoxes)
    {
        if (box.minY > torsoY + playerHeight * 0.25f)
            continue;

        if (box.intersects(nextX, torsoY, cameraZ, playerRadius, playerHeight * 0.5f))
        {
            blockedX = true;
            break;
        }
    }

    bool blockedZ = false;
    for (const auto &box : collisionBoxes)
    {
        if (box.minY > torsoY + playerHeight * 0.25f)
            continue;

        if (box.intersects(cameraX, torsoY, nextZ, playerRadius, playerHeight * 0.5f))
        {
            blockedZ = true;
            break;
        }
    }

    if (!blockedX)
        cameraX = nextX;
    if (!blockedZ)
        cameraZ = nextZ;

    // --- Collision verticale (sol) ---

    bool onGround = false;
    for (const auto &box : collisionBoxes)
    {
        float feetY = cameraY - playerHeight / 2.0f;

        bool insideXZ = (cameraX + playerRadius > box.minX) && (cameraX - playerRadius < box.maxX) &&
                        (cameraZ + playerRadius > box.minZ) && (cameraZ - playerRadius < box.maxZ);

        bool fallingOnto = (feetY >= box.maxY) && (feetY + verticalVelocity <= box.maxY);

        if (insideXZ && fallingOnto)
        {
            onGround = true;
            cameraY = box.maxY + playerHeight / 2.0f;
            verticalVelocity = 0.0f;
            isJumping = false;
            break;
        }
    }

    if (!onGround && !isJumping)
    {
        isJumping = true;
        verticalVelocity = 0.0f;
    }
}

void Camera::updateView()
{
    glLoadIdentity();
    gluLookAt(cameraX, cameraY, cameraZ,                                                             // position de la camera
              cameraX + sin(cameraAngleY), cameraY + sin(cameraAngleX), cameraZ + cos(cameraAngleY), // Point de regard
              0.0, 1.0, 0.0);                                                                        // direction du "haut"
}

void Camera::keyboardUp(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 'z':
    case 'Z':
        moveForward = false;
        break;
    case 's':
    case 'S':
        moveBackward = false;
        break;
    case 'q':
    case 'Q':
        moveLeft = false;
        break;
    case 'd':
    case 'D':
        moveRight = false;
        break;
    }
}

void Camera::keyboard(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 'z':
    case 'Z':
        moveForward = true;
        break;
    case 's':
    case 'S':
        moveBackward = true;
        break;
    case 'q':
    case 'Q':
        moveLeft = true;
        break;
    case 'd':
    case 'D':
        moveRight = true;
        break;
    case ' ':
        if (!isJumping)
        {
            isJumping = true;
            verticalVelocity = jumpSpeed;
        }
        break;
    }
}

void Camera::mouseMotion(int x, int y)
{

    int deltaX = x - lastX;
    int deltaY = y - lastY;

    cameraAngleY -= deltaX * rotationSpeed;
    cameraAngleX -= deltaY * rotationSpeed;

    if (cameraAngleX > 1.8f)
        cameraAngleX = 1.8f;
    if (cameraAngleX < -1.8f)
        cameraAngleX = -1.8f;

    glutWarpPointer(screenWidth / 2, screenHeight / 2);

    lastX = screenWidth / 2;
    lastY = screenHeight / 2;
}

float Camera::getXPosition()
{
    return cameraX;
}

float Camera::getYPosition()
{
    return cameraY;
}

float Camera::getZPosition()
{
    return cameraZ;
}

float Camera::getFrontX()
{
    return sin(cameraAngleY);
}

float Camera::getFrontY()
{
    return sin(cameraAngleX);
}

float Camera::getFrontZ()
{
    return cos(cameraAngleY);
}

float Camera::getPosition()
{
    return cameraX, cameraY, cameraZ;
}

float Camera::getFront()
{
    return sin(cameraAngleY), sin(cameraAngleX), cos(cameraAngleY);
}