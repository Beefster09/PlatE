
#include "gameloop.h"
#include "constants.h"
#include "sprite.h"
#include "hitbox.h"
#include "entity.h"
#include "either.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_timer.h>

using namespace PlatE;

int main(int argc, char* argv[]) {
	if (SDL_Init(SDL_INIT_EVERYTHING) >= 0) {
		SDL_Window* window = SDL_CreateWindow("PlatE", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 480, 0);
		SDL_Renderer* context = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		SDL_SetRenderDrawBlendMode(context, SDL_BLENDMODE_BLEND);

		// test display

		auto test = load_sprite_json("data/test.json");
		if (test.isLeft) {
			printf("Parsing failed: %s\n", test.left.description);
			SDL_Delay(5000);
			SDL_Quit();
			return 1;
		}

		//SDL_RenderClear(context);
		//render_hitboxes(context, { 100, 100 }, &test.right->framedata[0]);
		//render_hitboxes(context, { 300, 100 }, &test.right->framedata[1]);
		//render_hitboxes(context, { 100, 300 }, &test.right->framedata[2]);
		//render_hitboxes(context, { 300, 300 }, &test.right->framedata[3]);
		//SDL_RenderPresent(context);

		Engine engine;

		auto es1 = engine.add_entity_system(20);
		if (!es1.isLeft) {
			EntitySystem* system = es1.right;

			Entity temp{
				0, 0,
				{200, 400},
				{200, -1000},
				{0, 1000},
				test.right,
				&test.right->animations[0],
				0,
				0.0,
				&test.right->framedata[0]
			};

			add_entity(system, temp);
		}

		SDL_Event curEvent;
		uint32_t lastTime = SDL_GetTicks();
		while (true) {
			// process events
			while (SDL_PollEvent(&curEvent)) {
				if (curEvent.type == SDL_QUIT) {
					SDL_Quit();
					return EXIT_SUCCESS;
				}
				engine.event(curEvent);
			}

			// update... TODO: ability to choose delta time or static FPS
			uint32_t updateTime = SDL_GetTicks();
			engine.update(updateTime - lastTime);

			// render
			SDL_SetRenderDrawColor(context, 0, 0, 0, 255);
			SDL_RenderClear(context);

			engine.render(context);

			SDL_RenderPresent(context);

			int delay = 16 - (SDL_GetTicks() - lastTime);
			delay = (delay < 0) ? 0 : delay;
			lastTime = SDL_GetTicks();
			SDL_Delay(delay);
		}

		return EXIT_SDL_EVENT_FAIL;
	}
	else {
		printf("SDL init failed: %s\n", SDL_GetError());
		return EXIT_SDL_INIT_FAIL;
	}
}