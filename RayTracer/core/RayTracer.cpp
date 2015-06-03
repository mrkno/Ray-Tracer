#include "RayTracer.h"
#include "../objects/Cylinder.h"
#include "../objects/Cone.h"

/*
* This function compares the given ray with all objects in the scene
* and computes the closest point  of intersection.
*/
PointBundle RayTracer::closestPt(Vector pos, Vector dir)
{
	Vector point(0, 0, 0);
	float min = 10000.0;

	PointBundle out = {point, -1, 0.0};

	for (auto i = 0; i < sceneObjects.size(); i++)
	{
		auto t = sceneObjects[i]->intersect(pos, dir);
		if (t > 0) //Intersects the object
		{
			point = pos + dir * t;
			if (t < min)
			{
				out.point = point;
				out.index = i;
				out.dist = t;
				min = t;
			}
		}
	}

	return out;
}

/*
* Computes the colour value obtained by tracing a ray.
* If reflections and refractions are to be included, then secondary rays will
* have to be traced from the point, by converting this method to a recursive
* procedure.
*/
Colour RayTracer::trace(Vector pos, Vector dir, int step)
{
	auto q = closestPt(pos, dir);

	/*float fogAmount = fogProgression * (q.dist - fogStart);
	Colour background = backgroundCol;
	backgroundCol.combineColor(fogColourfogProgression);*/

	if (q.index == -1) return backgroundCol; //no intersection

	auto n = sceneObjects[q.index]->normal(q.point);
	auto l = light - q.point;
	l.normalise();
	float lDotn = l.dot(n); //Note �l� is the letter el, not the number 1.

	Colour col = sceneObjects[q.index]->getColour(q.point); //Object's colour

	Vector lightVector = light - q.point;
	float lightDist = lightVector.length(); //Distance to light
	lightVector.normalise();
	PointBundle s = closestPt(q.point, lightVector);

	Vector v(-dir.x, -dir.y, -dir.z); //View vector;

	Colour colourSum;
	float spec = 0.0f;

	if (s.index > -1 && s.dist < lightDist || lDotn <= 0)
	{
		colourSum = col.phongLight(backgroundCol, 0.0, 0.0);
		if (!sceneObjects[q.index]->isReflective())
		{
			return colourSum;
		}
	}
	else
	{
		Vector r = ((n * 2) * lDotn) - l; // r = 2(L.n)n � L. �l� = el
		r.normalise();
		float rDotv = r.dot(v);
		spec = rDotv < 0 ? 0.0 : pow(rDotv, 10); //Phong exponent = 10
		colourSum = col.phongLight(backgroundCol, lDotn, spec);
	}

	bool shouldReflect = false;
	if (sceneObjects[q.index]->isRefractive() && step < MAX_STEPS)
	{
		float n1 = 1.0;
		float n2 = sceneObjects[q.index]->getRefractiveIndex();

		float nRatio = n1 / n2;

		float thetat = sqrtf(1 - powf(nRatio, 2)*(1 - powf(dir.dot(n), 2)));
		
		Vector ns = n;
		ns.scale(nRatio * dir.dot(n) + cos(thetat));
		Vector dirs = dir;
		dirs.scale(nRatio);

		Vector g = dirs - ns;

		auto otherSide = closestPt(q.point, g);

		swap(n1, n2);
		nRatio = n1 / n2;

		thetat = sqrtf(1 - powf(nRatio, 2)*(1 - powf(dir.dot(n), 2)));

		ns = sceneObjects[q.index]->normal(q.point);
		ns.scale(-1);
		ns.scale(nRatio * dir.dot(n) + cos(thetat));
		dirs = g;
		dirs.scale(nRatio);

		g = dirs - ns;
		return trace(otherSide.point, g, step + 1);
		/*const float airRatio = 1.0f;


		float refractionCoeff = sceneObjects[q.index]->getRefractiveIndex();
		float nDotV = n.dot(v);

		float k = 1 - powf(refractionCoeff, 2) * (1 - powf(nDotV, 2));
		Vector refractionVector;
		if (k < 0.f)
		{
			shouldReflect = true;
			refractionVector = Vector(0, 0, 0);
		}
		else
		{
			Vector a = v, b = n;
			a.scale(refractionCoeff);
			b.scale(refractionCoeff * nDotV + sqrtf(k));
			refractionVector = a - b;
		}

		auto otherSide = closestPt(q.point, refractionVector);

		Colour refractionColour = trace(otherSide.point, dir, step + 1);
		//refractionColour.combineColor(colorSum, 0.50);
		colourSum = refractionColour;*/
	}

	// generate reflection ray
	if (shouldReflect || sceneObjects[q.index]->isReflective() && step < MAX_STEPS)
	{
		float reflCoeff = shouldReflect ? 1 : sceneObjects[q.index]->getReflectiveness();
		Vector reflectionVector = ((n * 2) * (n.dot(v))) - v;
		Colour reflectionColor = trace(q.point, reflectionVector, step + 1);
		if (reflCoeff == 1)
		{
			colourSum = reflectionColor.phongLight(backgroundCol, lDotn, spec);
		}
		else
		{
			colourSum.combineColor(reflectionColor, reflCoeff);
		}
	}

	return colourSum;
}

#if THREADS == 1

void RayTracer::display()
{
	int widthInPixels = WIDTH * PPU;
	int heightInPixels = HEIGHT * PPU;
	if (pixels != NULL)
	{
		delete [] pixels;
	}

	thread threads[THREAD_NUM];

	pixels = new Colour[widthInPixels * heightInPixels];

	glClear(GL_COLOR_BUFFER_BIT);

	for (int i = 0; i < THREAD_NUM; i++)
	{
		threads[i] = thread(displayMultithread, this, heightInPixels, widthInPixels, i, pixelSize);
	}

	// in separate loop to allow all threads to start up
	for (int i = 0; i < THREAD_NUM; i++)
	{
		if (threads[i].joinable()) threads[i].join();
	}

	float x, y;
	glBegin(GL_QUADS); //Each pixel is a quad.
	for (int i = 0; i < widthInPixels; i++)
	{
		x = XMIN + i * pixelSize;
		for (int j = 0; j < heightInPixels; j++)
		{
			y = YMIN + j * pixelSize;
			outputPixel(&pixels[i * widthInPixels + j], &x, &y);
		}
	}
	glEnd();
	glFlush();
}

void RayTracer::displayMultithread(RayTracer *tracer, int heightInPixels, int widthInPixels, int xStart, int pixelSize)
{
	float x, y;
	for (int i = xStart; i < widthInPixels; i+=THREAD_NUM)
	{
		x = tracer->XMIN + i * pixelSize;
		for (int j = 0; j < heightInPixels; j++)
		{
			y = tracer->YMIN + j * pixelSize;
			Colour colour = tracer->getColourNone(&x, &y, &tracer->halfPixelSize);
			tracer->pixels[i * widthInPixels + j] = colour;
		}
	}
}

#else

//---The main display module -----------------------------------------------------------
// In a ray tracing application, it just displays the ray traced image by drawing
// each pixel as quads.
//---------------------------------------------------------------------------------------
void RayTracer::display()
{
	cout << "Starting Redraw..." << endl << "0% complete.";
	int widthInPixels = WIDTH * PPU;
	int heightInPixels = HEIGHT * PPU;
	float x1, y1;

	int completionPix = widthInPixels / 100;
	int percent = 0;

	glClear(GL_COLOR_BUFFER_BIT);

	glBegin(GL_QUADS); //Each pixel is a quad.

	for (int i = 0; i < widthInPixels; i++) //Scan every "pixel"
	{
		x1 = XMIN + i * pixelSize;
		for (int j = 0; j < heightInPixels; j++)
		{
			y1 = YMIN + j * pixelSize;
			drawPixel(x1, y1);
		}
		
		if (i % completionPix == 0) cout << "\r" << percent++ << "% complete.";
	}

	glEnd();
	glFlush();
	cout << "\r100% complete." << endl << "Redraw Complete." << endl;
}

#endif


void RayTracer::outputPixel(Colour *col, float *x, float *y)
{
	glColor3f(col->r, col->g, col->b);
	glVertex2f(*x, *y); //Draw each pixel with its color value
	glVertex2f(*x + pixelSize, *y);
	glVertex2f(*x + pixelSize, *y + pixelSize);
	glVertex2f(*x, *y + pixelSize);
}

Colour RayTracer::getColourNone(float *x, float *y, float *halfSize)
{
	float xc = *x + *halfSize;
	float yc = *y + *halfSize;
	Vector dir(xc, yc, -edist); //direction of the primary ray
	dir.normalise(); //Normalise this direction
	return trace(eye, dir, 1); //Trace the primary ray and get the colour value
}

Colour RayTracer::getColourSupersample(float *x, float *y)
{
	Colour average(0, 0, 0);
	float frag_x = *x, frag_y;
	for (auto i = 0; i < samplingLevel; i++)
	{
		frag_y = *y;
		for (auto j = 0; j < samplingLevel; j++)
		{
			Vector direction(frag_x, frag_y, -edist);
			direction.normalise();
			average += trace(eye, direction, 1);
			frag_y += aaPixelSize;
		}
		frag_x += aaPixelSize;
	}
	average /= samplingSize;
	return average;
}

void RayTracer::drawPixel(float x, float y)
{
	Colour colour;
	switch (type)
	{
	default:
	case None:
	{
		colour = getColourNone(&x, &y, &halfPixelSize);
		break;
	}
	case Supersample: colour = getColourSupersample(&x, &y); break;
	}

	outputPixel(&colour, &x, &y);
}

RayTracer::RayTracer()
{
	fogProgression = 0.5 / (fogEnd - fogStart);

	glMatrixMode(GL_PROJECTION);
	gluOrtho2D(XMIN, XMAX, YMIN, YMAX);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClearColor(0, 0, 0, 1);

	/*Sphere* sphere1 = new Sphere(Vector(0, -5, -40), 3.0, Colour::RED);
	sphere1->setRefractiveIndex(1.333);
	Sphere* sphere2 = new Sphere(Vector(-10, 6, -100), 4.0, Colour::GREEN);
	sphere2->setReflectiveness(1);
	Sphere* sphere3 = new Sphere(Vector(5, 0, -100), 10.0, Colour::BLUE);
	sphere3->setReflectiveness(0.65);

	sceneObjects.push_back(sphere1);
	sceneObjects.push_back(sphere2);
	sceneObjects.push_back(sphere3);

	

	Cube* cube = new Cube(Vector(10, -7, -80), 2.5, Colour::GREEN);
	sceneObjects.push_back(cube);*/

	auto plane = new ChequeredFloor(Vector(-300, -10, 300), Vector(300, -10, 300),
	                         Vector(300, -10, -150), Vector(-300, -10, -150), Colour::WHITE, Colour::BLACK);
	sceneObjects.push_back(plane);

	//ProcedualSphere *procedual = new ProcedualSphere(Vector(-10, 0, -80), 2.5);
	//sceneObjects.push_back(procedual);

	ImageSphere *imageSphere = new ImageSphere(Vector(0, 0, -50), 5.0, "Moon.raw", 256, 128);
	sceneObjects.push_back(imageSphere);

	//Cylinder *cylinder = new Cylinder(Vector(0, -5, -50), 3, 2.5, Colour::RED);
	//sceneObjects.push_back(cylinder);

	//Cone *cone = new Cone(Vector(0, 0, -30), 6, 3, Colour::GREEN);
	//sceneObjects.push_back(cone);

}

RayTracer::~RayTracer()
{
	while (!sceneObjects.empty())
	{
		auto object = sceneObjects.back();
		sceneObjects.pop_back();
		delete object;
	}
}

void RayTracer::special(int key, int, int)
{
	switch (key)
	{
	case GLUT_KEY_UP:
	{
		if (edist == 100) break;
		edist += 1;
		break;
	}
	case GLUT_KEY_DOWN:
	{
		if (edist == 0) break;
		edist -= 1;
		break;
	}
	}
	glutPostRedisplay();
}

void RayTracer::key(unsigned char key, int, int)
{
	switch (key)
	{
		case 'A':
		case 'a':
		{
			int aaType = type;
			type = static_cast<AntiAliasType>((aaType + 1) % 2);
			cout << "Anti-Aliasing mode switched to: " << (type == 0 ? "Off" : "Supersample") << endl;
			glutPostRedisplay();
			break;
		}
#if THREADS == 1
		case 'T':
		case 't':
		{
			threads = !threads;
			cout << "Threading mode switched to: " << (threads ? "On" : "Off") << endl;
			break;
		}
#endif
		case 'Q':
		case 'q':
		{
			exit(0);
			break;
		}
	}
}
