#pragma once

#include "SDL_gpu.h"
#include <SDL2/SDL_events.h>
#include "result.h"
#include "error.h"
#include "entity.h"
#include "level.h"

#include "angelscript.h"

namespace Errors {
	const error_data
		ScriptCompileError = { 2000, "A script failed to compile." },
		ScriptNoSuchModule = { 2010, "A script module does not exist. [yet]" },
		ScriptNoSuchFunction = { 2011, "A certain module does not have a function with the specified name." },
		ScriptFunctionDidNotReturn = {2020, "A script function was expected to return a value but it didn't."};
}

constexpr float default_fps_min = 20;
constexpr float default_fps_max = 120;

namespace  Engine {
	void init();
	void update(int delta_time);
	void render(GPU_Target* context);
	void event(const SDL_Event& event);

	Result<asIScriptModule*> load_script(const char* filename);

	float get_time();
	void pause();
	void resume();

	void set_fps_range(float low, float high);

	int get_delay(int ticks_passed);

	bool travel(const std::string& levelname);

	void load_main_script();
}