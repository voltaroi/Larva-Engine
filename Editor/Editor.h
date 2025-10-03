#pragma once
#ifndef __EDITOR__
#define __EDITOR__
#include <iostream>
#include <GL/glut.h>
#include <vector>
#include <cstdlib>
#include <string>
#include <windows.h>

#include "Engine/UIButton.h"
#include "Engine/UI.h"
#include "Engine/TextBox.h"
#include "Engine/Camera.h"
#include "Engine/Sound.h"
#include "Engine/Quads.h"
#include "Engine/Triangles.h"
#include "Engine/Spheres.h"
#include "Engine/AABB.h"

class Editor
{
private:
	static int mouseX;
	static int mouseY;
	static bool mousePressed;
	static void blockMouse(int screenWidth, int screenHeight);
	static void runBuildBatch(std::vector<std::string>& outputLines);

	std::vector<AABB> cubes;

public:
	void init(int screenWidth, int screenHeight);
	void display();
	void update();
	void updateUI(int screenWidth, int screenHeight);

	static void globalKeyboard(unsigned char key, int x, int y);
	static void globalKeyboardUp(unsigned char key, int x, int y);
	static void globalMouseMotion(int x, int y);
	static void globalMouse(int button, int state, int x, int y);
};
#endif
