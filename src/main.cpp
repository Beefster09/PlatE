
#include "constants.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

int main(int argc, char* argv[]) {
	printf("Hello World!\n");

	if (SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Window* window = SDL_CreateWindow("PlatE", 100, 100, 800, 480, 0);

		return EXIT_SUCCESS;
	}
	else {
		return EXIT_SDL_FAIL;
	}
}