
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
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_surface.h>
#include "SDL_gpu.h"

#define SDL_INIT_CUSTOM (SDL_INIT_EVERYTHING)

#define BOOTLOADER_MAGIC_NUMBER "PlatEboot"

#define EXIT_SUCCESS 0

#define EXIT_BOOTLOADER_MISSING 1
#define EXIT_BOOTLOADER_BAD_HEADER 2
#define EXIT_BOOTLOADER_ERROR 3

#define EXIT_SDL_INIT_FAIL -1
#define EXIT_SDL_EVENT_FAIL -2
#define EXIT_SDL_GPU_FAIL -10

int main(int argc, char* argv[]) {
	if (SDL_Init(SDL_INIT_CUSTOM) >= 0) {
		const char* title;
		const char* iconfile;
		uint16_t virtual_width, virtual_height;
		try {
#define check(EXPR, CODE) do {auto res = (EXPR); if (!res) return CODE;} while(0)
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

			title = read_string<uint16_t>(stream);
			iconfile = read_string<uint16_t>(stream);

			// size of the virtual screen at all physical resolutions
			virtual_width = read<uint16_t>(stream);
			virtual_height = read<uint16_t>(stream);

			{
				char asset_dir[256];
				read_string(stream, read<uint16_t>(stream), asset_dir);
				if (!AssetManager::set_root_dir(asset_dir)) {
					return EXIT_BOOTLOADER_ERROR;
				}
			}

			{
				char script_dir[256];
				read_string<uint16_t>(stream, script_dir);
				// TODO: do something with it!
			}

			check(init_controller_types(stream), EXIT_BOOTLOADER_ERROR);
			check(init_controllers(stream), EXIT_BOOTLOADER_ERROR);
			// TODO: init controller instances and their binding defaults

			check(ColliderType::init(stream), EXIT_BOOTLOADER_ERROR);
			check(ColliderChannel::init(stream), EXIT_BOOTLOADER_ERROR);

			fclose(stream);
#undef check
		}
		catch (Error& e) {
			ERR("%s\n", std::to_string(e).c_str());
			return EXIT_BOOTLOADER_ERROR;
		}

		atexit(SDL_Quit);

		Engine::init("scripts/main.as");

		{
			auto r = load_config("settings.ini", Engine::getScriptEngine());
			if (!r) {
				std::string err = std::to_string(r.err);
				ERR("Error Loading Config: %s\n", err.c_str());
			}
		}
		auto& global_config = get_global_config();
		if (is_default(global_config.video.width)) global_config.video.width = virtual_width;
		if (is_default(global_config.video.height)) global_config.video.height = virtual_height;
		if (is_default(global_config.audio.master_volume)) global_config.audio.master_volume = 100;
		if (is_default(global_config.audio.sfx_volume)) global_config.audio.sfx_volume = 100;
		if (is_default(global_config.audio.sfx_volume)) global_config.audio.sfx_volume = 100;

		GPU_SetDebugLevel(GPU_DEBUG_LEVEL_MAX);

		GPU_Target* screen = GPU_Init(global_config.video.width, global_config.video.height, 0);
		if (screen == nullptr) {
			return EXIT_SDL_GPU_FAIL;
		}
		atexit(GPU_Quit);

		GPU_SetVirtualResolution(screen, virtual_width, virtual_height);

		// SDL_gpu apparently expects you to know its data structures instead of giving a proper function interface
		SDL_Window* window = SDL_GetWindowFromID(screen->context->windowID);
		SDL_Surface* icon = SDL_LoadBMP(iconfile);
		SDL_SetWindowTitle(window, title);
		if (icon != nullptr) SDL_SetWindowIcon(window, icon);

		// Flush the events so the window will show
		SDL_Event curEvent;
		while (SDL_PollEvent(&curEvent)) {
			if (curEvent.type == SDL_QUIT) {
				return EXIT_SUCCESS;
			}
		}

		Engine::start();

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