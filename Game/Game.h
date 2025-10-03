#pragma once
#ifndef __GAME__
#define __GAME__
#include <iostream>
#include <GL/glut.h>
#include <vector>
#include "Engine/Graphics/UIButton.h"
#include "Engine/Graphics/UI.h"
#include "Engine/Graphics/TextBox.h"
#include "Engine/Graphics/Camera.h"
#include "Engine/Graphics/Sound.h"
#include "Engine/Graphics/Quads.h"
#include "Engine/Graphics/Triangles.h"
#include "Engine/Graphics/Spheres.h"
#include "Engine/Core/AABB.h"
#include "Engine/Graphics/WindowUtils.h"
#include "Engine/Network/Client/Client.h"

class Game
{
private:
	static int mouseX;
	static int mouseY;
	static bool mousePressed;
	static void blockMouse(int screenWidth, int screenHeight);

	WindowUtils* windowUtils = nullptr;

	std::vector<AABB> cubes;

public:
	void init(int screenWidth, int screenHeight, WindowUtils& windowUtil);
	void display();
	void update();
	void updateUI(int screenWidth, int screenHeight);

	static void globalKeyboard(unsigned char key, int x, int y);
	static void globalKeyboardUp(unsigned char key, int x, int y);
	static void globalMouseMotion(int x, int y);
	static void globalMouse(int button, int state, int x, int y);
};
#endif
