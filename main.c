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

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>

// #include "cppjunk.h"
#include "pcg_basic.h"

#define ARRSIZE(ARRAY_) (sizeof(ARRAY_)/sizeof((ARRAY_)[0]))

#define TEMPLATE_NAME ".template"

int G_TIMESTEPS;
int G_WIDTH;
int G_HEIGHT;
int G_POINTS;

float G_DT; // "delta tee"

SDL_Window *G_Window;
SDL_Renderer *G_Renderer;

typedef union {
	struct {
		uint8_t r, g, b, a;
	};
	uint32_t color;
} Pixel;

typedef Pixel Color;

typedef struct {
	float px, py;
	float vx, vy;
	float ax, ay;
	Color color;
} Point;

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
Color GetRandomColor(Color *in)
{
	Color color = { 0 };

	color.r = RandBound(256);
	color.g = RandBound(256);
	color.b = RandBound(256);

	if (in != NULL) {
		color.r = (color.r + in->r) / 2;
		color.g = (color.g + in->g) / 2;
		color.b = (color.b + in->b) / 2;
	}

	color.a = 0xff;

	return color;
}

// GenerateRandomPoint: randomizes x and y, with a random color
void GenerateRandomPoint(Point *p)
{
	p->px = RandomFloat(G_WIDTH);
	p->py = RandomFloat(G_HEIGHT);
	p->vx = RandomFloat(30) - 60.0f;
	p->vy = RandomFloat(30) - 60.0f;
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
	*pixel = picked->color;
}

// DrawPoint: draws a colored dot at the point (x, y)
void DrawPoint(Pixel *pixels, int x, int y)
{
	for (int i = -2; i <= 2; i++) {
		for (int j = -2; j <= 2; j++) {
			if (x + i < 0 || x + i >= G_WIDTH)
				continue;
			if (y + j < 0 || y + j >= G_HEIGHT)
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

	point->vx += G_DT * point->ax;
	point->vy += G_DT * point->ay;

	point->px += G_DT * point->vx;
	point->py += G_DT * point->vy;
#undef OOB
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

bool MakeWindowTransparent()
{
	SDL_SysWMinfo wminfo;
	SDL_VERSION(&wminfo.version);
	SDL_GetWindowWMInfo(G_Window, &wminfo);
	HWND hwnd = wminfo.info.win.window;

	SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);

	return SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0xff, LWA_ALPHA);
}

int main(int argc, char **argv)
{
	int rc;

	// TODO (Brian) Get the screen resolution by calling Windows
	G_WIDTH = 1920;
	G_HEIGHT = 1080;
	G_TIMESTEPS = INT_MAX; // :)

	SeedRNG();

	G_POINTS = RandBound(7) + 5;

	Pixel *pixels = (Pixel *)calloc(G_WIDTH * G_HEIGHT, sizeof(*pixels));
	Point *points = (Point *)calloc(G_POINTS, sizeof(*points));

	for (int i = 0; i < G_POINTS; i++)
		GenerateRandomPoint(points + i);

	uint32_t flags = SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_ALWAYS_ON_TOP|SDL_WINDOW_BORDERLESS;

	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	rc = SDL_CreateWindowAndRenderer(G_WIDTH, G_HEIGHT, flags, &G_Window, &G_Renderer);
	if (rc < 0) {
		fprintf(stderr, "Couldn't Create Window or Renderer: %s\n", SDL_GetError());
		return 1;
	}

	SDL_SetWindowTitle(G_Window, "SDL Test");

	rc = SDL_SetRenderDrawBlendMode(G_Renderer, SDL_BLENDMODE_BLEND);
	if (rc < 0) {
		fprintf(stderr, "Couldn't set render blend mode: %s\n", SDL_GetError());
		return 1;
	}

	SDL_Event event;

	SDL_Texture *texture = SDL_CreateTexture(G_Renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, G_WIDTH, G_HEIGHT);

	uint32_t tstart, tend;
	int run = true;

	for (int t = 0; run && t < G_TIMESTEPS; t++) {
		tstart = SDL_GetTicks();

		while (SDL_PollEvent(&event) != 0) {
			switch (event.type) {
				case SDL_QUIT:
					run = false;
					break;

				case SDL_KEYDOWN:
				case SDL_KEYUP:
					if (event.key.keysym.scancode == SDL_SCANCODE_Q)
						run = false;
					break;

				default:
					break;
			}
		}

		for (int x = 0; x < G_WIDTH; x++) {
			for (int y = 0; y < G_HEIGHT; y++) {
				UpdatePixelForPoints(pixels + (x + G_WIDTH * y), x, y, points, G_POINTS);
			}
		}

		for (int i = 0; i < G_POINTS; i++) {
			DrawPoint(pixels, points[i].px, points[i].py);
		}

		SDL_UpdateTexture(texture, NULL, pixels, G_WIDTH * 4);

		SDL_SetRenderDrawColor(G_Renderer, 0, 0, 0, 0xff);
		SDL_RenderClear(G_Renderer);

		SDL_RenderCopy(G_Renderer, texture, NULL, NULL);

		MakeWindowTransparent();

		SDL_RenderPresent(G_Renderer);

		tend = SDL_GetTicks();

		G_DT = ((float)tend - tstart) / 1000.0f;

		for (int i = 0; i < G_POINTS; i++) {
			MovePoint(points + i);
		}
	}

	SDL_DestroyRenderer(G_Renderer);
	SDL_DestroyWindow(G_Window);

	SDL_Quit();

	free(points);
	free(pixels);

	return 0;
}
