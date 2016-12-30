
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

		GPU_Target* context = GPU_Init(800, 480, 0);
		if (context == nullptr) {
			return EXIT_SDL_GPU_FAIL;
		}

		// Flush the events so the window will show
		SDL_Event curEvent;
		while (SDL_PollEvent(&curEvent)) {
			if (curEvent.type == SDL_QUIT) {
				return EXIT_SUCCESS;
			}
		}

		atexit(GPU_Quit);

		CollisionType::init("");

		// test display

		auto test = load_sprite("data/test.sprite");
		if (!test) {
			printf("Parsing failed: %s\n", test.err.description);
			SDL_Delay(5000);
			return 1;
		}

		Engine& engine = Engine::get();

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
			GPU_Clear(context);

			engine.render(context);

			GPU_Flip(context);

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