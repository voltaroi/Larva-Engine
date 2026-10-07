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
public:
	enum class AnchorH { Left, Center, Right };
	enum class AnchorV { Bottom, Middle, Top };

private:
	static float textR;
	static float textG;
	static float textB;
	static float textA;

public:
	// Habillage optionnel de l'interface, choisi par le jeu (par défaut : rien ne change).
	//  - slant : les boîtes deviennent des parallélogrammes aux angles vifs, penchés vers la droite (décalage du
	//    haut par rapport au bas = slant x hauteur, borné par maxShift) ; sharp : angles vifs sans pencher
	//  - box, text, shape : filtres de couleur appliqués aux boîtes (drawBox), au texte (renderText) et aux
	//    tracés (Draw2D), pour changer de palette sans retoucher chaque appel
	//  - textHalo : liseré contrasté autour du texte, lisible sur un fond clair comme sur le décor
	struct Style
	{
		float slant = 0.0f, maxShift = 0.0f;
		float uprightAbove = 0.0f; // (hauteur à partir de laquelle une boîte reste d'aplomb : les fonds de panneau)
		float viewScale = 1.0f;    // (échelle du dessin en cours : l'écran fait alors largeur / viewScale unités)
		float buttonBar = 0.0f;    // (épaisseur de la barre d'accent sous chaque bouton, 0 : aucune)
		bool sharp = false, textHalo = false;
		void (*box)(float width, float height, float &r, float &g, float &b, float &a) = nullptr;
		void (*text)(float &r, float &g, float &b, float &a) = nullptr;
		void (*shape)(float &r, float &g, float &b, float &a) = nullptr;
	};
	static Style &style();

	static void setColor(float r, float g, float b, float a);
	static void loadfont(const char *fontPath);
	static void renderText(std::string text, float x, float y, float scale);
	static float getTextWidth(std::string text, float scale);
	static void drawText(float x, float y, const char *text, void *font = GLUT_BITMAP_HELVETICA_18);
	static void drawProgressBar(float x, float y, float width, float height, float percentage, float r, float g, float b);
	static void drawBox(float x, float y, float width, float height, float r, float g, float b, float alpha = 1.0f, bool border = false, float radius = 0.0f, AnchorH anchorH = AnchorH::Left, AnchorV anchorV = AnchorV::Bottom, int screenWidth = 0, int screenHeight = 0);
	static void drawImage(float x, float y, float width, float height, GLuint textureId, bool useOriginalSize, float opacity);
	static GLuint loadTexture(const char *path, bool nearest);
};

#endif
