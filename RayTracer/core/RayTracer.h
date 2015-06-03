#pragma once

#include <iostream>
#include <cmath>
#include <vector>
#include <GL/glut.h>
#include <GL/freeglut.h>

#if defined(_MSC_VER) && _MSC_VER >= 1700 || defined(__cplusplus) && __cplusplus > 199711L
#define THREADS 0
#else
#include <thread>
#define THREADS 1
#define THREAD_NUM 4
#endif

#include "AntiAliasManager.h"

#include "../objects/Object.h"
#include "../objects/Sphere.h"
#include "../objects/Plane.h"
#include "../objects/Cube.h"
#include "../objects/ChequeredFloor.h"
#include "../objects/ProcedualSphere.h"
#include "../objects/ImageSphere.h"

#include "../types/Colour.h"
#include "../types/Vector.h"
#include "../types/PointBundle.h"

using namespace std;

class RayTracer
{
public:
	// Constructors and Deconstructors
	RayTracer();
	~RayTracer();
	
	// Public Methods
	void display();
	void special(int, int, int);
	void key(unsigned char, int, int);
	Colour trace(Vector pos, Vector dir, int step);

private:
	// Private Constant Variables
	const float WIDTH = 20.0;
	const float HEIGHT = 20.0;
	const int PPU = 30;     //Total 600x600 pixels
	const int MAX_STEPS = 5;
	const float XMIN = -WIDTH * 0.5;
	const float XMAX = WIDTH * 0.5;
	const float YMIN = -HEIGHT * 0.5;
	const float YMAX = HEIGHT * 0.5;

	// Private Instance Variables
	vector<Object*> sceneObjects;
	Vector light = Vector(10.0, 40.0, -5.0);
	Colour backgroundCol = Colour::GRAY;
	Colour fogColour = Colour::WHITE;
	int fogStart = 80;
	int fogEnd = 120;
	AntiAliasType type = AntiAliasType::None;
	float pixelSize = 1.0 / PPU;
	Vector eye = Vector(0., 0., 0.);
	float halfPixelSize = pixelSize / 2.0;
	int samplingLevel = 4;
	int samplingSize = 16;
	float aaPixelSize = (samplingLevel / samplingSize) * pixelSize;
	float halfSupersamplePixelSize = (pixelSize / samplingLevel) / 2.0;
	float edist = 40.0;
	float fogProgression;

	// Private Methods
	PointBundle closestPt(Vector pos, Vector dir);
	void drawPixel(float x, float y);
	Colour getColourSupersample(float *x, float *y);
	Colour getColourNone(float *x, float *y, float *halfSize);
	void outputPixel(Colour *col, float *x, float *y);


#if THREADS == 1
	Colour* pixels;
	static void displayMultithread(RayTracer *tracer, int heightInPixels, int widthInPixels, int xStart, int pixelSize);
#endif
};

