// Brian Chrzanowski
// 2022-11-23 16:23:25
// Voronoi Diagram Image Generator

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
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
#include "pcg_basic.h"

#define ARRSIZE(ARRAY_) (sizeof(ARRAY_)/sizeof((ARRAY_)[0]))

#define TEMPLATE_NAME ".template"

int G_TIMESTEPS;
int G_WIDTH;
int G_HEIGHT;
int G_POINTS;
int G_THREADS;

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

typedef struct {
	int row;
	int rows;
	Pixel *pixels;
	Point *points;
	int *run;
	int *timestep;
	int *finished;
} ThreadData;

Color G_SkewColor = { 0xff, 0x00, 0x00, 0xff };

void SeedRNG()
{
	pcg32_srandom(time(NULL), (intptr_t)&SeedRNG);
}

uint32_t RandInt()
{
	return pcg32_random();
}

uint32_t RandBound(uint32_t bound)
{
	return pcg32_boundedrand(bound);
}

// RandomFloat: returns a random float
float RandomFloat(float max)
{
	return (float)RandInt() / ((float)UINT_MAX / max);
}

// GetRandomColor: returns a random color, pass a color to mix with the input color, or NULL to not
uint32_t GetRandomColor(Color *in)
{
	uint32_t color = 0;

	uint8_t red = RandBound(256);
	uint8_t green = RandBound(256);
	uint8_t blue = RandBound(256);

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
	p->vx = RandomFloat(5) - 10.0f;
	p->vy = RandomFloat(5) - 10.0f;
	// p->ax = RandomFloat(1) - 2.0f;
	// p->ay = RandomFloat(1) - 2.0f;
	p->color = GetRandomColor(&G_SkewColor);
}

float dist(float x1, float y1, float x2, float y2)
{
	return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}

// UpdatePixelForPoints
void UpdatePixelForPoints(Pixel *pixel, int x, int y, Point *points, size_t points_len)
{
	float x1, y1, x2, y2, xd, yd;
	float min = FLT_MAX;
	float currdist = 0;
	Point *picked = NULL;

	x1 = x;
	y1 = y;

	for (int i = 0; i < points_len; i++) {
#if 0
		// currdist = dist(x, y, points[i].px, points[i].py);
#else
		x2 = points[i].px;
		y2 = points[i].py;
		xd = x2 - x1;
		yd = y2 - y1;
		currdist = xd * xd + yd * yd;
#endif
		if (currdist < min) {
			min = currdist;
			picked = points + i;
		}
	}

	assert(picked != NULL);

	pixel->color = picked->color;
}

// DrawPoint: draws a colored dot at the point (x, y)
void DrawPoint(Pixel *pixels, int x, int y)
{
	for (int i = -2; i <= 2; i++) {
		for (int j = -2; j <= 2; j++) {
			if (x + i < 0 || x + i > G_WIDTH)
				continue;
			if (y + j < 0 || y + j > G_WIDTH)
				continue;
			pixels[(x + i) + G_WIDTH * (y + j)].g = 0xff;
		}
	}
}

// MovePoint: could be called 'UpdatePoint' moves the point according to our rules
void MovePoint(Point *point)
{
#define OOB(p, lo, hi) ((p) <= (lo) || (p) >= (hi))
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
#undef OOB
}

DWORD MultiPaintThreadProc(LPVOID param)
{
	ThreadData *data = param;

	Pixel *pixels = data->pixels;
	Point *points = data->points;

	int timestep = 0;

	for (;;) {
		BOOL wait = WaitOnAddress(data->timestep, &timestep, sizeof(*data->timestep), INFINITE);
		if (wait == FALSE) {
			if (data->run)
				continue;
			break;
		}

		for (int y = data->row; y < data->row + data->rows; y++) {
			for (int x = 0; x < G_WIDTH; x++) {
				UpdatePixelForPoints(pixels + (x + G_WIDTH * y), x, y, points, G_POINTS);
			}
		}

		InterlockedIncrement((LONG*)data->finished);
	}

	return 0;
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

int UpdateWallpaper(char *file)
{
	int rc;
	char fname[1024] = { 0 };

	memset(fname, 0, sizeof fname);

	size_t len = GetFullPathNameA(file, sizeof fname, fname, NULL);
	assert(len <= sizeof fname);

	rc = SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, fname, SPIF_UPDATEINIFILE|SPIF_SENDWININICHANGE);
	return rc ? 0 : -1;
}

void SingleThreaded(Pixel *pixels, Point *points)
{
	int rc;

	char image_name[256] = { 0 };
	snprintf(image_name, sizeof image_name, "%s.bmp", TEMPLATE_NAME);

	for (int t = 0; t < G_TIMESTEPS; t++) {
		printf("\rTimestep %d", t);

		for (int x = 0; x < G_WIDTH; x++) {
			for (int y = 0; y < G_HEIGHT; y++) {
				UpdatePixelForPoints(pixels + (x + G_WIDTH * y), x, y, points, G_POINTS);
			}
		}

		for (int i = 0; i < G_POINTS; i++) {
			DrawPoint(pixels, points[i].px, points[i].py);
		}

		rc = stbi_write_bmp(image_name, G_WIDTH, G_HEIGHT, 4, (void *)pixels);
		if (rc == 0) {
			fprintf(stderr, "There was an error writing the file!");
			exit(1);
		}

		rc = UpdateWallpaper(image_name);
		if (rc < 0) {
			fprintf(stderr, "Could not set wallpaper...\n");
			break;
		}

		for (int i = 0; i < G_POINTS; i++) {
			MovePoint(points + i);
		}
	}
}

void MultiThreaded(Pixel *pixels, Point *points)
{
	int run, timestep, finished, rc;

	G_THREADS = 20;

	char image_name[256] = { 0 };
	snprintf(image_name, sizeof image_name, "%s.bmp", TEMPLATE_NAME);

	HANDLE *threads = calloc(G_THREADS, sizeof(*threads));
	ThreadData *thread_data = calloc(G_THREADS, sizeof(*thread_data));

	{
		// NOTE (Brian) the threading model for this threading based attempt at speed is to delegate
		// a number of rows for the particular thread to complete. Because of this decision, it is
		// very important to choose a number that will EVENLY divide the vertical space, otherwise
		// we'll end up with un-updated rows (probably black in output??).

		for (int i = 0; i < G_THREADS; i++) {
			thread_data[i].row = i * (G_HEIGHT / G_THREADS);
			thread_data[i].rows = (G_HEIGHT / G_THREADS);
			thread_data[i].pixels = pixels;
			thread_data[i].points = points;
			thread_data[i].run = &run;
			thread_data[i].timestep = &timestep;
			thread_data[i].finished = &finished;
		}

		for (int i = 0; i < G_THREADS; i++) {
			threads[i] = CreateThread(NULL, 0, MultiPaintThreadProc, thread_data + i, 0, NULL);
			assert(threads[i] != NULL);
		}
	}

	run = true;

	for (int t = 0; t < G_TIMESTEPS; t++) {
		printf("\rTimestep %d", t);

		InterlockedExchange((LONG *)&finished, 0);
		InterlockedIncrement((LONG *)&timestep);

		while (InterlockedCompareExchange((LONG *)&finished, G_THREADS, 0) != G_THREADS)
			;

		for (int i = 0; i < G_POINTS; i++) {
			DrawPoint(pixels, points[i].px, points[i].py);
		}

		rc = stbi_write_bmp(image_name, G_WIDTH, G_HEIGHT, 4, (void *)pixels);
		if (rc == 0) {
			fprintf(stderr, "There was an error writing the file!");
			exit(1);
		}

		rc = UpdateWallpaper(image_name);
		if (rc < 0) {
			fprintf(stderr, "Could not set wallpaper...\n");
			break;
		}

		for (int i = 0; i < G_POINTS; i++) {
			MovePoint(points + i);
		}
	}

	run = false;
	InterlockedIncrement((LONG *)&timestep);

	for (int i = 0; i < G_THREADS; i++)
		WaitForSingleObject(threads[i], INFINITE);

	free(thread_data);
}

int main(int argc, char **argv)
{
	int rc;

	// TODO (Brian) Get the screen resolution by calling Windows
	G_WIDTH = 1280;
	G_HEIGHT = 720;
	G_TIMESTEPS = 1000; // :)

	SeedRNG();

	// G_POINTS = RandBound(14) + 5;
	G_POINTS = 6;

	Pixel *pixels = (Pixel *)calloc(G_WIDTH * G_HEIGHT, sizeof(*pixels));
	Point *points = (Point *)calloc(G_POINTS, sizeof(*points));

	for (int i = 0; i < G_POINTS; i++)
		GenerateRandomPoint(points + i);

#if 0
	SingleThreaded(pixels, points);
#else
	MultiThreaded(pixels, points);
#endif

	free(points);
	free(pixels);

	return 0;
}
