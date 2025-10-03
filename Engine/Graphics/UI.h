#pragma once
#ifndef __UI__
#define __UI__

#include <ft2build.h>
#include FT_FREETYPE_H
#include <GL/glut.h>
#include <map>
#include <string>
#include <cmath>
#include <iostream>

struct UICharacter
{
	GLuint textureID;
	unsigned int width;
	unsigned int height;
	int bearingX;
	int bearingY;
	unsigned int advance;
};

class UI
{
private:
	static float textR;
	static float textG;
	static float textB;
	static float textA;

public:
	static void setColor(float r, float g, float b, float a);
	static void loadfont(const char *fontPath);
	static void renderText(std::string text, float x, float y, float scale);
	static void drawText(float x, float y, const char *text, void *font = GLUT_BITMAP_HELVETICA_18);
	static void drawProgressBar(float x, float y, float width, float height, float percentage, float r, float g, float b);
	static void drawBox(float x, float y, float width, float height, float r, float g, float b, float alpha = 1.0f, bool border = false, float radius = 0.0f);
	static void drawImage(float x, float y, float width, float height, GLuint textureId, bool useOriginalSize, float opacity);
	static GLuint loadTexture(const char *path, bool nearest);
};

#endif
