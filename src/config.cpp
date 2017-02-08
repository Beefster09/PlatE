#include "config.h"
#include "cstrkey.h"
#include "ini.h"
#include "input.h"
#include <cstring>
#include <unordered_map>
#include <regex>

typedef std::unordered_map<cstr_key, asIScriptFunction*> SectionCallbacks;
static SectionCallbacks script_cfg_callbacks;
static Configuration global_config;

static int config_setting(void* user, const char* section, const char* key, const char* value);
static Configuration::config_switch str2switch(const char* str);

Result<> load_config(const char* file, asIScriptEngine* script_engine) {
	if (ini_parse(file, config_setting, script_engine) != 0) {
		return Error({ 30, "REPLACE ME" });
	}
	return Result<>::success;
}

Result<> save_config(const char* file, asIScriptEngine* script_engine) {
	return Result<>::success;
}


static int config_setting(void* user, const char* section, const char* key, const char* value) {
#define SECTION(SECNAME) (strcmp(section, SECNAME) == 0)
#define SPREFIX(PREFIX)  (strncmp(section, PREFIX, sizeof(PREFIX) - 1) == 0)
#define KEY(KEYNAME)     (strcmp(key, KEYNAME) == 0)
#define KPREFIX(PREFIX)  (strncmp(key, PREFIX, sizeof(PREFIX) - 1) == 0)
	if SECTION("Video") {
		if KEY("width") {
			global_config.video.resolution_x = atoi(value);
		}
		else if KEY("height") {
			global_config.video.resolution_y = atoi(value);
		}
		else if KEY("fullscreen") {
			global_config.video.fullscreen = str2switch(value);
		}
	}
	else if SECTION("Audio") {
		if KEY("master_volume") {
			global_config.audio.master_volume = atoi(value);
		}
		else if KEY("bgm_volume") {
			global_config.audio.bgm_volume = atoi(value);
		}
		else if KEY("sfx_volume") {
			global_config.audio.sfx_volume = atoi(value);
		}
		else if KEY("sfx_volume") {
			global_config.audio.stereo = str2switch(value);
		}
	}
	else if SPREFIX("Input_") {
		bind_from_ini(section + 6, key, value);
	}
	else if SPREFIX("Script_") {
		const cstr_key script_sec(section + 7);

		SectionCallbacks::const_iterator iter = script_cfg_callbacks.find(script_sec);
		if (iter == script_cfg_callbacks.end()) return 0;

		asIScriptFunction* callback = iter->second;
		asIScriptEngine* engine = reinterpret_cast<asIScriptEngine*>(user);
		asIScriptContext* ctx = engine->RequestContext();

		std::string script_key(key), script_value(value);

		ctx->Prepare(callback);
		ctx->SetArgObject(0, &script_key);
		ctx->SetArgObject(1, &script_value);
		int r = ctx->Execute();
		if (r == asEXECUTION_EXCEPTION) {
			ERR("Error in script config handler (function '%s' line %d): %s", ctx->GetExceptionFunction()->GetName(),
					ctx->GetExceptionLineNumber(), ctx->GetExceptionString());
		}
		ctx->Unprepare();
		engine->ReturnContext(ctx);
		return (r == asEXECUTION_FINISHED);
	}
	else {
		ERR_RELEASE("Unrecognized section: '%s'\n", section);
		return 0;
	}
	return 1;
#undef SECTION
#undef SPREFIX
#undef KEY
#undef KPREFIX
}

// A little heavyweight. Hopefully not a problem. Perhaps regexes will be needed elsewhere...
static const std::regex ON_PATTERN("on|1|t(rue)?|y(es)?|enabled");
static const std::regex OFF_PATTERN("off|0|f(alse)?|no?|disabled");
static Configuration::config_switch str2switch(const char* str) {
	char buf[16];
	strncpy(buf, str, 15);
	buf[16] = 0;
	if (std::regex_match(str, ON_PATTERN)) return Configuration::on;
	if (std::regex_match(str, OFF_PATTERN)) return Configuration::off;
	else return Configuration::inherit;
}

Configuration& get_global_config() { return global_config; }

static void AddSectionHandler(std::string& section, asIScriptFunction* callback) {
	script_cfg_callbacks.insert(std::make_pair(cstr_key(section.c_str(), true), callback));
}

#define check(EXPR) do {int r = (EXPR); assert(r >= 0);} while(0)
void RegisterConfigInterface(asIScriptEngine* engine) {
	check(engine->SetDefaultNamespace("Config"));

	check(engine->RegisterFuncdef("void SectionCallback(string &in, string &in)"));

	check(engine->RegisterGlobalFunction("void addSectionHandler(string &in, SectionCallback@)",
		asFUNCTION(AddSectionHandler), asCALL_CDECL));

	check(engine->SetDefaultNamespace(""));
}