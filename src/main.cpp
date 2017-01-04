
#include "engine.h"
#include "util.h"
#include "sprite.h"
#include "hitbox.h"
#include "entity.h"
#include "either.h"
#include "level.h"
#include "assetmanager.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include "SDL_gpu.h"
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_timer.h>

using namespace PlatE;

#define SDL_INIT_CUSTOM (SDL_INIT_EVERYTHING & ~SDL_INIT_VIDEO | SDL_INIT_EVENTS)

int main(int argc, char* argv[]) {
	if (SDL_Init(SDL_INIT_CUSTOM) >= 0) {
		atexit(SDL_Quit);

		GPU_Target* screen = GPU_Init(800, 480, 0);
		if (screen == nullptr) {
			return EXIT_SDL_GPU_FAIL;
		}
		atexit(GPU_Quit);

		// Flush the events so the window will show
		SDL_Event curEvent;
		while (SDL_PollEvent(&curEvent)) {
			if (curEvent.type == SDL_QUIT) {
				return EXIT_SUCCESS;
			}
		}

		CollisionType::init("");

		Engine& engine = Engine::get();

		engine.init();

		uint32_t lastTime = SDL_GetTicks();
		while (true) {
			// process events
			while (SDL_PollEvent(&curEvent)) {
				if (curEvent.type == SDL_QUIT) {
					return EXIT_SUCCESS;
				}
				engine.event(curEvent);
			}

			// update... TODO: ability to choose delta time or static FPS
			uint32_t updateTime = SDL_GetTicks();
			engine.update(updateTime - lastTime);

			// render
			GPU_Clear(screen);

			engine.render(screen);

			GPU_Flip(screen);

			// TODO: better FPS/delay calculation
			int delay = 16 - (SDL_GetTicks() - lastTime);
			delay = (delay < 0) ? 0 : delay;
			lastTime = updateTime;
			SDL_Delay(delay);
		}

		return EXIT_SDL_EVENT_FAIL;
	}
	else {
		printf("SDL init failed: %s\n", SDL_GetError());
		return EXIT_SDL_INIT_FAIL;
	}
}