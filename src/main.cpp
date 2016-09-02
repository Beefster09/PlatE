
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

int main(int argc, char* argv[]) {
	if (SDL_Init(SDL_INIT_EVERYTHING) >= 0) {
		SDL_Window* window = SDL_CreateWindow("PlatE", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 480, 0);
		SDL_Renderer* context = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		SDL_SetRenderDrawBlendMode(context, SDL_BLENDMODE_BLEND);

		//SDL_StartTextInput();

		// Make a simple sprite frame with some hitboxes...
		FrameData test;

		test.n_hitboxes = 4;
		test.hitboxes = new Hitbox[4];
		for (int i = 0; i < 4; ++i) {
			test.hitboxes[i].shape = BOX;
			test.hitboxes[i].box.x = -20;
			test.hitboxes[i].box.y = -20;
			test.hitboxes[i].box.w =  40;
			test.hitboxes[i].box.h =  50;
		}
		test.hitboxes[0].box.x = -25;
		test.hitboxes[1].box.y = -30;
		test.hitboxes[2].box.w =  60;
		test.hitboxes[3].box.h =  80;

		test.hitboxes[0].type = SOLID;
		test.hitboxes[1].type = HURTBOX;
		test.hitboxes[2].type = DAMAGE;
		test.hitboxes[3].type = BLOCK;

		// test display

		SDL_RenderClear(context);
		render_hitboxes(context, { 100, 100 }, test);
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

			// update... TODO: choose delta time or static FPS
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