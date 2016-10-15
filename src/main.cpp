
#include "engine.h"
#include "macros.h"
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

		CollisionType::init("");

		// test display

		auto test = load_sprite_json("data/test.sprite.json");
		if (test.isLeft) {
			printf("Parsing failed: %s\n", test.left.description);
			SDL_Delay(5000);
			SDL_Quit();
			return 1;
		}

		Engine engine;

		auto es1 = engine.add_entity_system(20);
		if (!es1.isLeft) {
			EntitySystem* system = es1.right;

			const EntityClass* temp_class = new EntityClass{
				"LOLOLOLOL",
				test.right,
				0,
				true
			};

			auto blah = spawn_entity(system, temp_class, {100, 400});

			if (!blah.isLeft) {
				blah.right->velocity = { 300, -1000 };
				blah.right->gravity = { 0, 1500 };
			}

			auto blah2 = spawn_entity(system, temp_class, { 400, 350 });

			if (!blah2.isLeft) {
				blah2.right->velocity = { 25, -10 };
				blah2.right->gravity = { 0, 20 };
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