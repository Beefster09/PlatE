
#include "gameloop.h"
#include "constants.h"
#include "sprite.h"
#include "display.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_timer.h>

using namespace GlobalGameLoop;

int main(int argc, char* argv[]) {
	if (SDL_Init(SDL_INIT_EVERYTHING) >= 0) {

		auto test = load_sprite_json("data/test.json");
		if (test.isLeft) {
			printf("Parsing failed: %s\n", test.left.description);
			SDL_Delay(5000);
			SDL_Quit();
			return 1;
		}

		SDL_Window* window = SDL_CreateWindow("PlatE", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 480, 0);
		SDL_Renderer* context = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		SDL_SetRenderDrawBlendMode(context, SDL_BLENDMODE_BLEND);

		// test display

		SDL_RenderClear(context);
		render_hitboxes(context, { 100, 100 }, test.right->framedata[0]);
		render_hitboxes(context, { 300, 100 }, test.right->framedata[1]);
		render_hitboxes(context, { 100, 300 }, test.right->framedata[2]);
		render_hitboxes(context, { 300, 300 }, test.right->framedata[3]);
		SDL_RenderPresent(context);

		SDL_Event curEvent;
		uint32_t lastTime = 0;
		while (true) {
			// process events
			while (SDL_PollEvent(&curEvent)) {
				if (curEvent.type == SDL_QUIT) {
					SDL_Quit();
					return EXIT_SUCCESS;
				}
				event(curEvent);
			}

			// update... TODO: ability to choose delta time or static FPS
			uint32_t updateTime = SDL_GetTicks();
			update(updateTime - lastTime);

			// render
			render(context);

			int delay = 16 - (SDL_GetTicks() - lastTime);
			delay = (delay < 0) ? 0 : delay;
			SDL_Delay(delay);
			printf("Delaying %dms.\n", delay);
			lastTime = SDL_GetTicks();
		}

		return EXIT_SDL_EVENT_FAIL;
	}
	else {
		printf("SDL init failed: %s\n", SDL_GetError());
		return EXIT_SDL_INIT_FAIL;
	}
}