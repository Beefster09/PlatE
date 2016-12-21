
#include "engine.h"
#include "macros.h"
#include "sprite.h"
#include "hitbox.h"
#include "entity.h"
#include "either.h"
#include "level.h"
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

		CollisionType::init("");

		// test display

		auto test = load_sprite("data/test.sprite");
		if (!test) {
			printf("Parsing failed: %s\n", test.err.description);
			SDL_Delay(5000);
			SDL_Quit();
			return 1;
		}

		auto level = load_level("data/test.level");
		if (!level) {
			printf("Parsing failed: %s\n", level.err.description);
			SDL_Delay(5000);
			SDL_Quit();
			return 1;
		}

		Engine engine;

		auto mod = engine.load_script("scripts/main.as");
		if (!mod) {
			printf("Failed to load script 'main.as': %s\n", mod.err.description);
		}
		else {
			auto result = engine.run_script_function("scripts/main.as", "init");
			if (!result) {
				printf("error %d: %s\n", result.err.code, result.err.description);
			}
			else {
				printf("Result of init: %d\n", result.value);
			}
		}

		auto es1 = engine.init_entity_system(20);
		if (es1) {
			EntitySystem* system = es1.value;

			const EntityClass* temp_class = new EntityClass{
				"LOLOLOLOL",
				test.value,
				0
			};

			auto blah = spawn_entity(system, temp_class, {100, 400});

			if (blah) {
				blah.value->velocity = { 300, -1000 };
				blah.value->gravity = { 0, 1500 };
			}

			auto blah2 = spawn_entity(system, temp_class, { 400, 350 });

			if (blah2) {
				blah2.value->velocity = { 25, -10 };
				blah2.value->gravity = { 0, 20 };
			}
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

			// TODO: better FPS/delay calculation
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