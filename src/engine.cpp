
#include <SDL2/SDL_timer.h>

#include "engine.h"
#include "entity.h"
#include "vectors.h"
#include "fileutil.h"
#include "rng.h"
#include "input.h"
#include "config.h"

#include "angelscript.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"
#include "scriptarray/scriptarray.h"
#include "scriptdictionary/scriptdictionary.h"
#include "scriptany/scriptany.h"
#include "scriptbuilder/scriptbuilder.h"

namespace Engine {
	// === GLOBAL VARIABLES ===
	static EntitySystem* entity_system = nullptr;

	static LevelInstance* active_level = nullptr;

	static asIScriptEngine* script_engine = nullptr;

	static asIScriptFunction* scriptfunc_init = nullptr;
	static asIScriptFunction* scriptfunc_start = nullptr;
	static asIScriptFunction* scriptfunc_update = nullptr;

	static CScriptBuilder script_builder;

	static bool paused = false; // MAYBE: some sort of enum so that GUI stuff (optionally) works when paused
	static uint32_t init_time = 0;

	static float min_timestep, max_timestep;
	static float tick_remainder = 0.f;

	// === Scripting interface ===
	static void load_main_script(const char* main_script);

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

	void init(const char* main_script) {
#define check(EXPR) do {int r = (EXPR); assert(r >= 0);} while (0)
		set_fps_range(default_fps_min, default_fps_max);
		// init Scripting API
		script_engine = asCreateScriptEngine();

		script_engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);

		RegisterScriptMath(script_engine);

		check(script_engine->RegisterGlobalProperty("const float PI", const_cast<float*>(&pi)));
		check(script_engine->RegisterGlobalProperty("const float TAU", const_cast<float*>(&tau)));

		RegisterScriptArray(script_engine, true);
		RegisterStdString(script_engine);
		RegisterStdStringUtils(script_engine);
		RegisterScriptDictionary(script_engine);

		// This will probably be useful for game saves... Or maybe I can do better?
		RegisterScriptAny(script_engine);

		// Util
		check(script_engine->RegisterGlobalFunction("void println(string& in)",
			asFUNCTIONPR(PrintLn, (const std::string&), void), asCALL_CDECL));
		check(script_engine->RegisterGlobalFunction("void println(int64)",
			asFUNCTIONPR(PrintLn, (int64_t), void), asCALL_CDECL));
		check(script_engine->RegisterGlobalFunction("void println(float)",
			asFUNCTIONPR(PrintLn, (float), void), asCALL_CDECL));
		check(script_engine->RegisterGlobalFunction("void println(bool)",
			asFUNCTIONPR(PrintLn, (bool), void), asCALL_CDECL));

		// Error handling (async callbacks)
		check(script_engine->RegisterFuncdef("void ErrorCallback(int, const string &in)"));

		// Game types
		RegisterConfigInterface(script_engine);

		RegisterVector2(script_engine);
		RegisterRandomTypes(script_engine);

		check(RegisterColliderTypes(script_engine));
		RegisterEntityTypes(script_engine);

		RegisterInputTypes(script_engine);
		check(RegisterControllerTypes(script_engine));

		// Global engine stuff
		check(script_engine->SetDefaultNamespace("Engine"));

		check(script_engine->RegisterGlobalFunction("float get_time()",
			asFUNCTION(get_time), asCALL_CDECL));

		check(script_engine->RegisterGlobalFunction("void pause()",
			asFUNCTION(pause), asCALL_CDECL));
		check(script_engine->RegisterGlobalFunction("void resume()",
			asFUNCTION(resume), asCALL_CDECL));

		check(script_engine->RegisterGlobalFunction("bool travel(const string &in)",
			asFUNCTION(travel), asCALL_CDECL));

		check(script_engine->SetDefaultNamespace(""));

		// ==================================================================
		// Now that the script engine is ready, we can initialize the entity system
		entity_system = new EntitySystem();
		check(script_engine->RegisterGlobalProperty("__EntitySystem__ EntitySystem", entity_system));

		// Now onto customizable initialization
		load_main_script(main_script);
	}
#undef check

	void start() {
		auto ctx = script_engine->RequestContext();

		ctx->Prepare(scriptfunc_start);

		int r = ctx->Execute();
		if (r == asEXECUTION_FINISHED) {
			ctx->Unprepare();
			script_engine->ReturnContext(ctx);
		}
		else {
			ERR_RELEASE("Fatal error: global start script did not return.");
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
			ERR_RELEASE("Fatal error: global tick script did not return.");
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

#define check(EXPR, ERROR) do {int r = (EXPR); if (r < 0) return ERROR;} while(0)
	Result<asIScriptModule*> load_script(const char* filename) {
		check(script_builder.StartNewModule(script_engine, filename), Errors::ScriptCompileError);
		check(script_builder.AddSectionFromFile(filename), Errors::ScriptCompileError);
		check(script_builder.BuildModule(), Errors::ScriptCompileError);

		// TODO: metadata things

		return script_builder.GetModule();
	}
#undef check

	void load_main_script(const char* main_script_filename) {
		auto main_script = load_script(main_script_filename);
		if (main_script) {
			auto module = main_script.value;

			asIScriptFunction* init_func = module->GetFunctionByDecl("void init()");

			if (init_func != nullptr) {
				auto ctx = script_engine->RequestContext();

				ctx->Prepare(init_func);

				int r = ctx->Execute();
				if (r == asEXECUTION_FINISHED) {
					ctx->Unprepare();
					script_engine->ReturnContext(ctx);
				}
				else {
					ERR_RELEASE("Fatal error: void init() did not return.\n");
					abort();
				}
			}
			else {
				ERR("init function not found. Ignoring.\n");
			}

			scriptfunc_start = module->GetFunctionByDecl("void start()");
			scriptfunc_update = module->GetFunctionByDecl("void update(float)");

			if (scriptfunc_start == nullptr) {
				ERR_RELEASE("Fatal error: void start() is missing.\n");
			}

			if (scriptfunc_update == nullptr) {
				ERR_RELEASE("Fatal error: void update(float) is missing.\n");
			}
		}
		else {
			ERR_RELEASE("Fatal error: could not load main script!\n");
			abort();
		}
	}

	asIScriptEngine* getScriptEngine() { return script_engine; }

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