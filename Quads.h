#ifndef __QUADS__
#define __QUADS__
#include "AABB.h"

struct Vec3
{
	float x = 0;
	float y = 0;
	float z = 0;
};

class Quads
{
private:
	AABB quad;

	Vec3 rotation;
	Vec3 position;
	Vec3 scale;
	Vec3 color;
	bool HasColor = false;

	const char *name;

public:
	Quads();
	void draw();
	void setRotation(float x, float y, float z);
	void addRotation(float x, float y, float z);
	void setPosition(float x, float y, float z);
	void addPosition(float x, float y, float z);
	void setScale(float x, float y, float z);
	void setColor(float r, float g, float b);
	AABB getAABB() const;
	void setName(const char *newName);
	const char *getName();
	Vec3 getPosition();
	Vec3 getRotation();
};

#endif