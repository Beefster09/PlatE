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

namespace PlatE {

	class Engine {
	private:
		EntitySystem* entity_system = nullptr;

		const Level* active_level = nullptr;
		// Level* peripheral_level; // used for splicing levels together
		// Viewport* camera;
		// InputSystem* input;

		asIScriptEngine* script_engine = nullptr;

		asIScriptFunction* scriptfunc_init = nullptr;
		asIScriptFunction* scriptfunc_update = nullptr;

		bool paused = false; // MAYBE: some sort of enum so that GUI stuff works when paused
		uint32_t init_time = 0;

		float min_timestep, max_timestep;
		float tick_remainder = 0.f;

		Engine();

		static Engine singleton;
	public:
		Engine(const Engine& other) = delete;
		Engine(Engine&& other) = delete;
		Engine& operator = (Engine& other) = delete;

		__forceinline static Engine& get() { return singleton; }

		void init();
		void update(int delta_time);
		void render(GPU_Target* context);
		void event(const SDL_Event& event);

		Result<asIScriptModule*> load_script(const char* filename);
		Result<asDWORD> run_script_function_byname(const char* modname, const char* funcname);

		float get_time();
		inline void pause() { paused = true; }
		inline void resume() { paused = false; }

		void set_fps_range(float low, float high);

		int get_delay(int ticks_passed);

		bool travel(const std::string& levelname);

	private:
		void load_main_script();
	};

	void exit();
}