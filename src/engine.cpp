
#include <SDL2\SDL_mutex.h>

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

static void MessageCallback(const asSMessageInfo *msg, void *param)
{
	const char *type = "ERR ";
	if (msg->type == asMSGTYPE_WARNING)
		type = "WARN";
	else if (msg->type == asMSGTYPE_INFORMATION)
		type = "INFO";
	printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

static void PrintLn(std::string& str) {
	printf("%s\n", str.c_str());
}

Engine::Engine() {
	entity_system = nullptr;

	// init Scripting API
	script_engine = asCreateScriptEngine();
	
	script_engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);

	RegisterScriptMath(script_engine);

	RegisterVector2(script_engine);

	RegisterScriptArray(script_engine, true);
	RegisterStdString(script_engine);
	RegisterStdStringUtils(script_engine);
	RegisterScriptDictionary(script_engine);

	int r = script_engine->RegisterGlobalFunction("void println(string& in)", asFUNCTION(PrintLn), asCALL_CDECL); assert(r >= 0);

	// This will probably be useful for game saves... Or maybe I can do better?
	RegisterScriptAny(script_engine);

	//context_pool = ContextPool(script_engine);
}

void Engine::update(int delta_time) {
	process_entities(delta_time, entity_system);
}

void Engine::render(SDL_Renderer* context) {
	render_entities(context, entity_system);
}

void Engine::event(const SDL_Event& event) {

}

Option<Error> Engine::load_script(const char* filename) {
	auto res = open(filename, "r");
	if (!res.isLeft) {
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
			return nullptr;
		}
	}
	else return res.left;
}

Either<Error, asDWORD> Engine::run_script_function(const char* modname, const char* funcname) {
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

Either<Error, EntitySystem*> Engine::init_entity_system(size_t capacity) {
	auto result = create_entity_system(capacity);

	if (result.isLeft) {
		return result.left;
	}
	else {
		if (entity_system != nullptr) {
			auto res = destroy_entity_system(entity_system);
			if (res) {
				return res.value;
			}
		}

		entity_system = result.right;
		return result.right;
	}
}

//ContextPool::ContextPool(asIScriptEngine* engine) {
//	this->engine = engine;
//	
//	lock = SDL_CreateMutex();
//	assert(lock != nullptr);
//
//	pool = DenseBucket<entry>(CONTEXT_POOL_CAPACITY);
//}
//
//ContextPool::~ContextPool() {
//	if (lock != nullptr) SDL_DestroyMutex(lock);
//
//	pool.free();
//}
//
///// Gets an RAII context handle. Synchronized.
//ContextPool::handle& ContextPool::getContext() {
//	if (/*SDL_LockMutex(lock) == 0*/ true) {
//		for (auto& ctx : pool) {
//			if (ctx.active) continue;
//
//			handle result(this, &ctx);
//			if (/*SDL_UnlockMutex(lock) == 0*/true) {
//				return result;
//			}
//			else {
//				return handle();
//			}
//		}
//		auto res = pool.add(entry{ false, engine->CreateContext() });
//		if (res.isLeft) {
//			return handle();
//		}
//		else {
//			return handle(this, res.rightOrElse(nullptr));
//		}
//	}
//	else {
//		return handle();
//	}
//}
//
//ContextPool::handle::handle(ContextPool* pool, ContextPool::entry* ctx) {
//	this->pool = pool;
//	this->ctx = ctx;
//	if (pool != nullptr && ctx != nullptr) {
//		ctx->active = true;
//	}
//}
//
//ContextPool::handle::handle(handle&& other) :
//	pool(nullptr), ctx(nullptr)
//{
//	pool = other.pool;
//	ctx = other.ctx;
//	other.ctx = nullptr;
//	other.pool = nullptr;
//}
//
//ContextPool::handle::~handle() {
//	//assert(SDL_LockMutex(pool->lock) == 0 && "Unable to lock mutex in destructor");
//	ctx->active = false;
//	//assert(SDL_UnlockMutex(pool->lock) == 0 && "Unable to unlock mutex in destructor");
//	printf("Handle returned to pool");
//}