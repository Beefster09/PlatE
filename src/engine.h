#pragma once

#include <SDL2/SDL_render.h>
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

	//class ContextPool {
	//	class handle;
	//	struct entry {
	//		bool active;
	//		asIScriptContext* context;
	//	};

	//	asIScriptEngine* engine;
	//	SDL_mutex* lock;
	//	DenseBucket<entry> pool;

	//public:

	//	class handle {
	//		ContextPool* pool;
	//		entry* ctx;

	//	public:
	//		handle(ContextPool* pool, entry* ctx);
	//		handle(handle&& other); // MOVE only
	//		inline handle() { pool = nullptr; ctx = nullptr; }
	//		~handle();

	//		inline operator bool() const { return pool != nullptr && ctx != nullptr; }
	//		inline operator asIScriptContext*() const { return ctx->context; }
	//		inline asIScriptContext* operator -> () { return ctx->context; }
	//	};

	//	ContextPool(asIScriptEngine* engine);
	//	ContextPool() {}
	//	~ContextPool();

	//	/// Gets an RAII context handle. Synchronized.
	//	handle& getContext();
	//};

	class Engine {
	private:
		EntitySystem* entity_system;
		Level* active_level;
		// Level* peripheral_level; // used for splicing levels together
		// Viewport* camera;
		// InputSystem* input;
		// AssetManager asset_manager;

		asIScriptEngine* script_engine;

		//ContextPool context_pool;
	public:
		Result<EntitySystem*> init_entity_system(size_t size = ENTITY_SYSTEM_DEFAULT_SIZE);

		void update(int delta_time);
		void render(SDL_Renderer* context);
		void event(const SDL_Event& event);

		Result<> load_script(const char* filename);
		Result<asDWORD> run_script_function(const char* modname, const char* funcname);

		Engine();
	};

	void exit();
}