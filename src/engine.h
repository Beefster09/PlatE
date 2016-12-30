#pragma once

#include "SDL_gpu.h"
#include <SDL2/SDL_events.h>
#include "either.h"
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

#define CONTEXT_POOL_CAPACITY 32

namespace PlatE {

	class Engine {
	private:
		EntitySystem* entity_system;

		const Level* active_level;
		// Level* peripheral_level; // used for splicing levels together
		// Viewport* camera;
		// InputSystem* input;
		// AssetManager asset_manager;

		asIScriptEngine* script_engine;

		asIScriptFunction* scriptfunc_init;
		asIScriptFunction* scriptfunc_update;

		asIScriptContext* script_main_context;

		//ContextPool context_pool;

		bool paused = false; // MAYBE: some sort of enum so that GUI stuff works when paused
		uint32_t init_time = 0;

		Engine();

		static Engine singleton;
	public:
		Engine(const Engine& other) = delete;
		Engine(Engine&& other) = delete;
		Engine& operator = (Engine& other) = delete;

		__forceinline static Engine& get() { return singleton; }

		Result<EntitySystem*> init_entity_system(size_t size = ENTITY_SYSTEM_DEFAULT_SIZE);

		void init();
		void update(int delta_time);
		void render(GPU_Target* context);
		void event(const SDL_Event& event);

		Result<asIScriptModule*> load_script(const char* filename);
		Result<asDWORD> run_script_function_byname(const char* modname, const char* funcname);

		float get_time();
		inline void pause() { paused = true; }
		inline void resume() { paused = false; }

		bool travel(const std::string& levelname);

	private:
		void load_main_script();
	};

	void exit();
}