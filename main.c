// Brian Chrzanowski
// 2022-11-23 16:23:25
// Voronoi Diagram Image Generator

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <assert.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Shobjidl.h>

#include "cppjunk.h"

#define ARRSIZE(ARRAY_) (sizeof(ARRAY_)/sizeof((ARRAY_)[0]))

int G_TIMESTEPS;
int G_WIDTH;
int G_HEIGHT;
int G_POINTS;

typedef struct {
	float px, py;
	float vx, vy;
	float ax, ay;
	uint32_t color;
} Point;

typedef struct {
	uint8_t r, g, b, a;
} Color;

typedef union {
	struct {
		uint8_t r, g, b, a;
	};
	uint32_t color;
} Pixel;

Color G_SkewColor = { 0xff, 0x00, 0xff, 0xff };

// RandomInt: returns a random integer
int RandomInt()
{
	return rand(); // REPLACE MEEE
}

// RandomFloat: returns a random float
float RandomFloat(float max)
{
	return (float)rand() / (float)(RAND_MAX / max);
}

// GetRandomColor: returns a random color, pass a color to mix with the input color, or NULL to not
uint32_t GetRandomColor(Color *in)
{
	uint32_t color = 0;

	uint8_t red = RandomInt() % 256;
	uint8_t green = RandomInt() % 256;
	uint8_t blue = RandomInt() % 256;

	if (in != NULL) {
		red = (red + in->r) / 2;
		green = (green + in->g) / 2;
		blue = (blue + in->b) / 2;
	}

	color |= 0xff;
	color <<= 8;
	color |= blue;
	color <<= 8;
	color |= green;
	color <<= 8;
	color |= red;

	return color;
}

// GenerateRandomPoint: randomizes x and y, with a random color
void GenerateRandomPoint(Point *p)
{
	p->px = RandomFloat(G_WIDTH);
	p->py = RandomFloat(G_HEIGHT);
	p->vx = (RandomFloat(5) - 10.0f) / 5.0f;
	p->vy = (RandomFloat(5) - 10.0f) / 5.0f;
	// p->ax = RandomFloat(1) - 2.0f;
	// p->ay = RandomFloat(1) - 2.0f;
	p->color = GetRandomColor(&G_SkewColor);
}

float dist(float x1, float y1, float x2, float y2)
{
	return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}

void UpdatePixelForPoints(Pixel *pixel, int x, int y, Point *points, size_t points_len)
{
	float min = FLT_MAX;
	Point *picked = NULL;

	for (int i = 0; i < points_len; i++) {
		float currdist = dist(x, y, points[i].px, points[i].py);
		if (currdist < min) {
			min = currdist;
			picked = points + i;
		}
	}

	assert(picked != NULL);

	pixel->color = picked->color;
}

int OOB(float p, float lo, float hi)
{
	return p <= lo || p >= hi;
}

// MovePoint: could be called 'UpdatePoint' moves the point according to our rules
void MovePoint(Point *point)
{
	if (OOB(point->py, 0, G_HEIGHT)) {
		point->ay *= -1.0f;
		point->vy *= -1.0f;
	}

	if (OOB(point->px, 0, G_WIDTH)) {
		point->ax *= -1.0f;
		point->vx *= -1.0f;
	}

	point->vx += point->ax;
	point->vy += point->ay;

	point->px += point->vx;
	point->py += point->vy;
}

void PrintPoint(Point *point, int idx)
{
	printf("Point %d\n", idx);
	printf("  PX %3.4f\n", point->px);
	printf("  PY %3.4f\n", point->py);
	printf("  VX %3.4f\n", point->vx);
	printf("  VY %3.4f\n", point->vy);
	printf("  AX %3.4f\n", point->ax);
	printf("  AY %3.4f\n", point->ay);
}

int main(int argc, char **argv)
{
	int rc;
	char *basename = "VoronoiDiagram";
	char sbuf[256];

	if (argc == 2) {
		basename = argv[1];
	}

#if 0
	G_WIDTH = GetSystemMetrics(SM_CXFULLSCREEN);
	G_HEIGHT = GetSystemMetrics(SM_CYFULLSCREEN);
	G_HEIGHT = 1080; // figure out how to get real monitor dimensions
	G_TIMESTEPS = 4;
#else
	G_WIDTH = 640;
	G_HEIGHT = 480;
	G_TIMESTEPS = 100; // :)
#endif

	G_POINTS = rand() % 24;

	srand(time(NULL));

	Pixel *pixels = (Pixel *)calloc(G_WIDTH * G_HEIGHT, sizeof(*pixels));
	Point *points = (Point *)calloc(G_POINTS, sizeof(*points));

	for (int i = 0; i < G_POINTS; i++) {
		GenerateRandomPoint(points + i);
	}

	for (int t = 0; t < G_TIMESTEPS; t++) {
		for (int x = 0; x < G_WIDTH; x++) {
			for (int y = 0; y < G_HEIGHT; y++) {
				UpdatePixelForPoints(pixels + (x + G_WIDTH * y), x, y, points, G_POINTS);
			}
		}

		memset(sbuf, 0, sizeof sbuf);
		snprintf(sbuf, sizeof sbuf, "%s_%d.jpg", basename, t + 1);

		rc = stbi_write_jpg(sbuf, G_WIDTH, G_HEIGHT, 4, (void *)pixels, 90);
		if (rc == 0) {
			fprintf(stderr, "There was an error writing the file!");
			exit(1);
		}

		for (int i = 0; i < G_POINTS; i++) {
			MovePoint(points + i);
		}
	}

	free(points);
	free(pixels);

#if 0
	GUID DesktopWallpaperGuid = GetCOMInterfaceGUIDFromName("DesktopWallpaper");
	if (IsGUIDZero(DesktopWallpaperGuid)) {
		// error
	}

	GUID IDesktopWallpaperGuid = GetCOMInterfaceGUIDFromName("IDesktopWallpaper");
	if (IsGUIDZero(IDesktopWallpaperGuid)) {
		// error
	}


	// Now that we have the file, we'll attempt to set the wallpaper
	DWORD result;
	HRESULT hresult;
	char fullname[1024];
	LPWSTR *monitor_id;

	CoInitialize(NULL);

	result = GetFullPathNameA(fname, sizeof fullname, fullname, NULL);
	assert(result <= sizeof fullname); // ??, an error condition

	GUID DesktopWallpaperGuid;
	GUID IDesktopWallpaperGuid;

	GUIDFromStringA("c2cf3110-460e-4fc1-b9d0-8a1c0c9cc4bd", &DesktopWallpaperGuid);
	GUIDFromStringA("b92b56a9-8b55-4e14-9a89-0199bbb6f93b", &IDesktopWallpaperGuid);

	IDesktopWallpaper *desktop;

	if (SUCCEEDED(CoCreateInstance(&DesktopWallpaperGuid, 0, CLSCTX_LOCAL_SERVER, &IDesktopWallpaperGuid, (void**)&desktop))) {
		desktop->SetWallpaper(NULL, fullname);
		desktop->Release();
	} else {
		fprintf(stderr, "Couldn't change wallapper!\n");
	}

	CoUninitialize();
#endif

	return 0;
}
