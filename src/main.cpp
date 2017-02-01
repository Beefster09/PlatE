
#include "engine.h"
#include "util.h"
#include "sprite.h"
#include "hitbox.h"
#include "entity.h"
#include "result.h"
#include "level.h"
#include "assetmanager.h"
#include "input.h"
#include "fileutil.h"
#include "config.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include "SDL_gpu.h"
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_timer.h>

#define SDL_INIT_CUSTOM (SDL_INIT_EVERYTHING & ~SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#define BOOTLOADER_MAGIC_NUMBER "PlatEboot"

#define EXIT_SUCCESS 0

#define EXIT_BOOTLOADER_MISSING 1
#define EXIT_BOOTLOADER_BAD_HEADER 2
#define EXIT_BOOTLOADER_ERROR 3

#define EXIT_SDL_INIT_FAIL -1
#define EXIT_SDL_EVENT_FAIL -2
#define EXIT_SDL_GPU_FAIL -10

#define check(EXPR, CODE) {auto res = (EXPR); if (!res) return CODE;}
int main(int argc, char* argv[]) {
	const char* title;
	const char* asset_dir;
	try {
		auto bootloader = open("engine.boot", "rb");

		if (!bootloader) {
			ERR("Unable to open engine.boot: %s", std::to_string(bootloader.err).c_str());
			return EXIT_BOOTLOADER_MISSING;
		}
		FILE* stream = bootloader;

		if (!check_header(stream, "PlatEboot")) {
			ERR("Bootloader did not start with \"" BOOTLOADER_MAGIC_NUMBER "\"");
			return EXIT_BOOTLOADER_BAD_HEADER;
		}

		title = read_string(stream, read<uint16_t>(stream));

		asset_dir = read_string(stream, read<uint16_t>(stream));
		if (!AssetManager::set_root_dir(asset_dir)) {
			return EXIT_BOOTLOADER_BAD_HEADER;
		}
		delete[] asset_dir;

		check(init_controller_types(stream), EXIT_BOOTLOADER_ERROR);

		check(ColliderType::init(stream), EXIT_BOOTLOADER_ERROR);
		check(ColliderChannel::init(stream), EXIT_BOOTLOADER_ERROR);

		fclose(stream);
	}
	catch (Error& e) {
		ERR("%s\n", std::to_string(e).c_str());
		return EXIT_BOOTLOADER_ERROR;
	}

	if (SDL_Init(SDL_INIT_CUSTOM) >= 0) {
		atexit(SDL_Quit);

		GPU_SetDebugLevel(GPU_DEBUG_LEVEL_MAX);

		GPU_Target* screen = GPU_Init(800, 480, 0);
		if (screen == nullptr) {
			return EXIT_SDL_GPU_FAIL;
		}
		atexit(GPU_Quit);
		// SDL_gpu apparently expects you to know its data structures instead of giving a proper function interface
		SDL_Window* window = SDL_GetWindowFromID(screen->context->windowID);
		SDL_SetWindowTitle(window, title);

		{
			ControllerInstance* inst = create_controller("Menu", "test_controller");
			RealInput input;

			input.set_key(SDL_SCANCODE_UP);
			bind_button(inst, 0, input);

			input.set_key(SDL_SCANCODE_W);
			bind_button(inst, 0, input);

			input.set_key(SDL_SCANCODE_DOWN);
			bind_button(inst, 1, input);

			input.set_key(SDL_SCANCODE_S);
			bind_button(inst, 1, input);

			input.set_key(SDL_SCANCODE_RETURN);
			bind_button(inst, 4, input);

			input.set_key(SDL_SCANCODE_SPACE);
			bind_button(inst, 4, input);
		}

		// Flush the events so the window will show
		SDL_Event curEvent;
		while (SDL_PollEvent(&curEvent)) {
			if (curEvent.type == SDL_QUIT) {
				return EXIT_SUCCESS;
			}
		}

		Engine::init();

		uint32_t lastTime = SDL_GetTicks();
		while (true) {
			// process events
			while (SDL_PollEvent(&curEvent)) {
				if (curEvent.type == SDL_QUIT) {
					return EXIT_SUCCESS;
				}
				Engine::event(curEvent);
			}

			uint32_t updateTime = SDL_GetTicks();
			Engine::update(updateTime - lastTime);

			// render
			GPU_Clear(screen);

			Engine::render(screen);

			GPU_Flip(screen);

			int delay = Engine::get_delay(SDL_GetTicks() - lastTime);
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