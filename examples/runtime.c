#include <stdio.h>
#include <SDL/SDL.h>

extern double _anc_main();

int main(int argc, char **argv)
{
	printf("%f\n", _anc_main());
	return 0;
}

#define WIDTH 512
#define HEIGHT 512
#define DEPTH 24

static SDL_Surface *screen;

double sdl_pixel(double x, double y, double r, double g, double b)
{
	unsigned char *p = (unsigned char*)screen->pixels +
		(int)y * screen->pitch +
		(int)x * screen->format->BytesPerPixel;

	*p++ = 255*r;
	*p++ = 255*g;
	*p++ = 255*b;
	return 0.0;
}

double sdl_init()
{
	SDL_Init(SDL_INIT_VIDEO);
	screen = SDL_SetVideoMode(WIDTH, HEIGHT, DEPTH, SDL_SWSURFACE);
	return 0.0;
}

double sdl_flip()
{
	SDL_Flip(screen);
	return 0.0;
}

double sdl_loop()
{
	SDL_Event e;
	while (1) {
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_QUIT:
				return 0.0;
			case SDL_KEYDOWN:
				return 0.0;
			}
		}
	}
}
