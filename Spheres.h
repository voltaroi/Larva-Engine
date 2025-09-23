#ifndef __SPHERES__
#define __SPHERES__

class Spheres
{
private:
	struct Position {
		float x = 0;
		float y = 0;
		float z = 0;
	};
	struct Rotation {
		float x = 0;
		float y = 0;
		float z = 0;
	};
	struct Scale {
		float x = 0;
		float y = 0;
		float z = 0;
	};

	Rotation rotation;
	Position position;
	Scale scale;

public:
	Spheres();
	void draw();
	void setRotation(float, float, float);
	void addRotation(float, float, float);
	void setPosition(float, float, float);
	void addPosition(float, float, float);
	void setScale(float, float, float);
};

#endif
