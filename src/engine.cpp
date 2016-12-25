
//#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_timer.h>

#include "engine.h"
#include "entity.h"
#include "vectors.h"
#include "fileutil.h"

#include "angelscript.h"
#include "scriptmath\scriptmath.h"
#include "scriptstdstring\scriptstdstring.h"
#include "scriptarray\scriptarray.h"
#include "scriptdictionary\scriptdictionary.h"
#include "scriptany\scriptany.h"

using namespace PlatE;

Engine Engine::singleton;

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

static void PrintLn(long i) {
	printf("%ld\n", i);
}

static void PrintLn(float f) {
	printf("%g\n", f);
}

static const float pi = static_cast<float>(M_PI);
static const float tau = static_cast<float>(M_PI * 2);

Engine::Engine() {
	entity_system = nullptr;
	script_engine = nullptr;
}

void Engine::init() {
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
		asFUNCTIONPR(PrintLn, (long), void), asCALL_CDECL); assert(r >= 0);
	r = script_engine->RegisterGlobalFunction("void println(float)",
		asFUNCTIONPR(PrintLn, (float), void), asCALL_CDECL); assert(r >= 0);

	// Game Stuff
	RegisterVector2(script_engine);

	// Global engine stuff
	r = script_engine->RegisterObjectType("__Engine__", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

	r = script_engine->RegisterGlobalProperty("__Engine__ Engine", this);

	r = script_engine->RegisterObjectMethod("__Engine__", "float get_time()",
		asMETHOD(Engine, get_time), asCALL_THISCALL); assert(r >= 0);

	r = script_engine->RegisterObjectMethod("__Engine__", "void pause()",
		asMETHOD(Engine, pause), asCALL_THISCALL); assert(r >= 0);
	r = script_engine->RegisterObjectMethod("__Engine__", "void resume()",
		asMETHOD(Engine, resume), asCALL_THISCALL); assert(r >= 0);

	r = script_engine->RegisterObjectMethod("__Engine__", "bool travel(string)",
		asMETHOD(Engine, travel), asCALL_THISCALL); assert(r >= 0);

	// Now onto customizable initialization
	load_main_script();

	script_main_context = script_engine->CreateContext();

	script_main_context->Prepare(scriptfunc_init);

	r = script_main_context->Execute();
	if (r == asEXECUTION_FINISHED) {
		script_main_context->Unprepare();
	}
	else {
		perror("Fatal error: init script did not return.");
		abort();
	}

	init_time = SDL_GetTicks();
}

void Engine::update(int delta_time) {
	float delta_seconds = static_cast<float>(delta_time) / 1000.f;
	if (!paused) {
		process_entities(delta_seconds, entity_system);
	}

	script_main_context->Prepare(scriptfunc_update);
	script_main_context->SetArgFloat(0, delta_seconds);

	int r = script_main_context->Execute();
	if (r == asEXECUTION_FINISHED) {
		script_main_context->Unprepare();
	}
	else {
		perror("Fatal error: tick script did not return.");
		abort();
	}
}

void Engine::render(GPU_Target* context) {
	render_tilemap(context, &active_level->layers[0]);
	render_entities(context, entity_system);
}

void Engine::event(const SDL_Event& event) {

}

float Engine::get_time() {
	return static_cast<float>(SDL_GetTicks() - init_time) / 1000.f;
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

void Engine::load_main_script() {
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

Result<asDWORD> Engine::run_script_function_byname(const char* modname, const char* funcname) {
	asIScriptModule* module = script_engine->GetModule(modname);
	if (module == nullptr) return Errors::ScriptNoSuchModule;
	asIScriptFunction* function = module->GetFunctionByName(funcname);
	if (function == nullptr) return Errors::ScriptNoSuchFunction;

	// TODO: context pooling

	auto ctx = script_engine->CreateContext();
	ctx->Prepare(function);

	int r = ctx->Execute();
	if (r == asEXECUTION_FINISHED) {
		asDWORD return_value = ctx->GetReturnDWord();
		ctx->Release();
		return return_value;
	}
	else {
		ctx->Release();
		return Errors::ScriptFunctionDidNotReturn;
	}
}

void PlatE::exit() {
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

Result<EntitySystem*> Engine::init_entity_system(size_t capacity) {
	auto result = create_entity_system(capacity);

	if (result) {
		if (entity_system != nullptr) {
			auto res = destroy_entity_system(entity_system);
			if (!res) {
				return res.err;
			}
		}

		entity_system = result.value;
		return result.value;
	}
	else return result.err;
}

bool Engine::travel(std::string levelname) {
	auto res = load_level(levelname.c_str());

	if (res) {
		active_level = res.value;
		return true;
	}
	else return false;
}