
#include <SDL2/SDL_timer.h>

#include "engine.h"
#include "entity.h"
#include "vectors.h"
#include "fileutil.h"
#include "rng.h"
#include "input.h"

#include "angelscript.h"
#include "scriptmath\scriptmath.h"
#include "scriptstdstring\scriptstdstring.h"
#include "scriptarray\scriptarray.h"
#include "scriptdictionary\scriptdictionary.h"
#include "scriptany\scriptany.h"

namespace Engine {
	// === GLOBAL VARIABLES ===
	static EntitySystem* entity_system = nullptr;

	static LevelInstance* active_level = nullptr;

	static asIScriptEngine* script_engine = nullptr;

	static asIScriptFunction* scriptfunc_init = nullptr;
	static asIScriptFunction* scriptfunc_update = nullptr;

	static bool paused = false; // MAYBE: some sort of enum so that GUI stuff works when paused
	static uint32_t init_time = 0;

	static float min_timestep, max_timestep;
	static float tick_remainder = 0.f;

	// === Scripting interface ===
	const char* MAIN_SCRIPT_FILENAME = "scripts/main.as"; // Subject to change

	static void MessageCallback(const asSMessageInfo *msg, void *param)
	{
		const char *type = "ERR ";
		if (msg->type == asMSGTYPE_WARNING)
			type = "WARN";
		else if (msg->type == asMSGTYPE_INFORMATION)
			type = "INFO";
		printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
	}

	static void PrintLn(const std::string& str) {
		printf("%s\n", str.c_str());
	}

	static void PrintLn(int64_t i) {
		printf("%lld\n", i);
	}

	static void PrintLn(float f) {
		printf("%g\n", f);
	}

	static void PrintLn(bool b) {
		printf("%s\n", b ? "true" : "false");
	}

	static const float pi = static_cast<float>(M_PI);
	static const float tau = static_cast<float>(M_PI * 2);

	// === Core functionality ===

	void init() {
		set_fps_range(default_fps_min, default_fps_max);
		// init Scripting API
		script_engine = asCreateScriptEngine();

		script_engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);

		int r;

		RegisterScriptMath(script_engine);

		r = script_engine->RegisterGlobalProperty("const float PI", const_cast<float*>(&pi)); assert(r >= 0);
		r = script_engine->RegisterGlobalProperty("const float TAU", const_cast<float*>(&tau)); assert(r >= 0);

		RegisterScriptArray(script_engine, true);
		RegisterStdString(script_engine);
		RegisterStdStringUtils(script_engine);
		RegisterScriptDictionary(script_engine);

		// This will probably be useful for game saves... Or maybe I can do better?
		RegisterScriptAny(script_engine);

		// Util
		r = script_engine->RegisterGlobalFunction("void println(string& in)",
			asFUNCTIONPR(PrintLn, (const std::string&), void), asCALL_CDECL); assert(r >= 0);
		r = script_engine->RegisterGlobalFunction("void println(int64)",
			asFUNCTIONPR(PrintLn, (int64_t), void), asCALL_CDECL); assert(r >= 0);
		r = script_engine->RegisterGlobalFunction("void println(float)",
			asFUNCTIONPR(PrintLn, (float), void), asCALL_CDECL); assert(r >= 0);
		r = script_engine->RegisterGlobalFunction("void println(bool)",
			asFUNCTIONPR(PrintLn, (bool), void), asCALL_CDECL); assert(r >= 0);

		// Error handling (async callbacks)
		r = script_engine->RegisterFuncdef("void ErrorCallback(int, const string &in)"); assert(r >= 0);

		// Game types
		RegisterVector2(script_engine);
		RegisterRandomTypes(script_engine);

		r = RegisterColliderTypes(script_engine); assert(r >= 0);
		RegisterEntityTypes(script_engine);

		RegisterInputTypes(script_engine);
		r = RegisterControllerTypes(script_engine); assert(r >= 0);

		// Global engine stuff
		const char* ns = script_engine->GetDefaultNamespace();
		r = script_engine->SetDefaultNamespace("Engine"); assert(r >= 0);

		r = script_engine->RegisterGlobalFunction("float get_time()",
			asFUNCTION(get_time), asCALL_CDECL); assert(r >= 0);

		r = script_engine->RegisterGlobalFunction("void pause()",
			asFUNCTION(pause), asCALL_CDECL); assert(r >= 0);
		r = script_engine->RegisterGlobalFunction("void resume()",
			asFUNCTION(resume), asCALL_CDECL); assert(r >= 0);

		r = script_engine->RegisterGlobalFunction("bool travel(const string &in)",
			asFUNCTION(travel), asCALL_CDECL); assert(r >= 0);

		r = script_engine->SetDefaultNamespace(ns); assert(r >= 0);

		// ==================================================================
		// Now that the script engine is ready, we can initialize the entity system
		entity_system = new EntitySystem();
		r = script_engine->RegisterGlobalProperty("__EntitySystem__ EntitySystem", entity_system); assert(r >= 0);

		// Now onto customizable initialization
		load_main_script();

		auto ctx = script_engine->RequestContext();

		ctx->Prepare(scriptfunc_init);

		r = ctx->Execute();
		if (r == asEXECUTION_FINISHED) {
			ctx->Unprepare();
			script_engine->ReturnContext(ctx);
		}
		else {
			perror("Fatal error: global init script did not return.");
			abort();
		}

		init_time = SDL_GetTicks();
	}

	void update(int delta_time) {
		float delta_seconds = static_cast<float>(delta_time) / 1000.f;
		if (delta_seconds > max_timestep) delta_seconds = max_timestep;

		update_inputs(delta_seconds);

		entity_system->executor.run_deferred();

		if (!paused) {
			entity_system->update(script_engine, active_level, delta_seconds);
		}

		auto ctx = script_engine->RequestContext();

		ctx->Prepare(scriptfunc_update);
		ctx->SetArgFloat(0, delta_seconds);

		int r = ctx->Execute();
		if (r == asEXECUTION_FINISHED) {
			ctx->Unprepare();
			script_engine->ReturnContext(ctx);
		}
		else {
			perror("Fatal error: global tick script did not return.");
			abort();
		}
	}

	void render(GPU_Target* screen) {
		if (active_level != nullptr) {
			render_tilemap(screen, &(active_level->layers[0]));
		}

		// TODO: interleaving

		auto entities = entity_system->render_iter();
		for (auto iter = entities.first; iter != entities.second; ++iter) {
			(*iter)->render(screen);
		}
	}

	void event(const SDL_Event& event) {

	}

	float get_time() {
		return static_cast<float>(SDL_GetTicks() - init_time) / 1000.f;
	}

	void set_fps_range(float low, float high) {
		min_timestep = 1000.f / high;
		max_timestep = 1000.f / low;
		tick_remainder = 0.f;
	}

	int get_delay(int ticks_passed) {
		float ticks;
		tick_remainder = modf(min_timestep + tick_remainder, &ticks);
		int delay = static_cast<int>(ticks) - ticks_passed;
		return delay > 0 ? delay : 0;
	}

	Result<asIScriptModule*> Engine::load_script(const char* filename) {
		auto res = open(filename, "r");
		if (res) {
			const char* script = read_all(res);
			if (script == nullptr) return Errors::IncompleteFileRead;

			asIScriptModule* module = script_engine->GetModule(filename, asGM_ALWAYS_CREATE);
			module->AddScriptSection("main", script, strlen(script));

			int r = module->Build();
			delete[] script;

			if (r < 0) {
				// there were errors
				return Errors::ScriptCompileError;
			}
			else {
				return module;
			}
		}
		else return res.err;
	}

	void load_main_script() {
		auto main_script = load_script(MAIN_SCRIPT_FILENAME);
		if (main_script) {
			auto module = main_script.value;

			scriptfunc_init = module->GetFunctionByDecl("void init()");

			scriptfunc_update = module->GetFunctionByDecl("void update(float)");

			if (scriptfunc_init == nullptr) {
				LOG_RELEASE("Fatal error: void init() is missing.\n");
			}

			if (scriptfunc_update == nullptr) {
				LOG_RELEASE("Fatal error: void update(float) is missing.\n");
			}
		}
		else {
			perror("Fatal error: could not load main script!\n");
			abort();
		}
	}

	void pause() { paused = true; }
	void resume() { paused = false; }

	bool travel(const std::string& levelname) {
		auto res = load_level(levelname.c_str());

		if (res) {
			if (active_level != nullptr) {
				destroy_level_instance(active_level);
			}
			active_level = instantiate_level(res.value);
			return true;
		}
		else return false;
	}
}