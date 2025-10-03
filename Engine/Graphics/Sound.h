#ifndef __SOUND__
#define __SOUND__
#include <thread>
#include <chrono>
#include <cmath>
#include <sndfile.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <iostream>
#include <vector>
#include <GL/glut.h>
#include "Camera.h"

#define M_PI 3.14159265358979323846

class Sound
{
private:
	ALuint source;
    ALfloat sourcePosition[3];
	float volumeMulti = 1;
	float maxDistance = 50;

public:
	void play(const char* filename, float x, float y, float z);
	void updateListenerPosition(Camera &camera);
	void checkALError();
	ALuint load(const char*);
	void setVolumeMulti(float);
	void setMaxDistance(float);
};

#endif
