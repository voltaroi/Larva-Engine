#ifndef __CAMERA__
#define __CAMERA__
#include <cmath>
#include <GL/glut.h>
#include <chrono>
#include <vector>
#include "../Core/AABB.h"

class Camera
{
private:
	int screenWidth;
	int screenHeight;
	float playerRadius = 0.5f;
	float playerHeight = 1.8f;;
    

public:
	void init(int, int);
	void update(const std::vector<AABB>& collisionBoxes);
	void networkUpdate();
	void snapToPosition(float x, float y, float z);
	void updateView();
	void keyboard(unsigned char, int, int);
	void keyboardUp(unsigned char, int, int);
	void mouseMotion(int, int);

	void setPosition(float x, float y, float z);

	float getXPosition();
	float getYPosition();
	float getZPosition();

	float getFrontX();
	float getFrontY();
	float getFrontZ();
	float getPosition();
	float getFront();
};
#endif
