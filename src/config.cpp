#include "config.h"
#include "cstrkey.h"
#include "ini.h"
#include "input.h"
#include "fileutil.h"
#include <cstring>
#include <unordered_map>
#include <regex>
#include <type_traits>

struct callback_pair {
	asIScriptFunction *reader, *writer;
};

struct config_var {
	void* ptr;
	int type_id;
};

typedef std::unordered_map<cstr_key, callback_pair> SectionCallbacks;
typedef std::unordered_map<cstr_key, config_var> AutoVars;

static SectionCallbacks script_cfg_callbacks;
static AutoVars auto_vars;
static Configuration global_config;

Configuration& get_global_config() { return global_config; }

static int config_setting(void* user, const char* section, const char* key, const char* value);
static config_switch str2switch(const char* str);

#pragma region Utility Templates

/// assign config value (int types implementation)
template <typename T>
static typename std::enable_if<std::is_integral<T>::value, bool>::type
assign_str(void* ptr, const char* str) {
	errno = 0;
	uint64_t val;
	char* end;

	if (std::is_unsigned<T>::value) {
		val = strtoull(str, &end, 10);
	}
	else {
		val = strtoll(str, &end, 10);
	}

	if (errno) {
		ERR("Failed conversion from const char* to %s: %s\n", typeid(T).name(), str);
		return false;
	}
	else {
		*reinterpret_cast<T*>(ptr) = static_cast<T>(val);
		return true;
	}
}

template <typename T>
static typename std::enable_if<std::is_floating_point<T>::value, bool>::type
assign_str(void* ptr, const char* str) {
	errno = 0;
	T val;
	char* end;
	if (std::is_same<T, float>::value) {
		val = strtof(str, &end);
	}
	else if (std::is_same<T, double>::value) {
		val = strtod(str, &end);
	}
	else {
		return false;
	}

	if (errno) {
		ERR("Failed conversion from const char* to %s: %s\n", typeid(T).name(), str);
		return false;
	}
	else {
		*reinterpret_cast<T>(ptr) = static_cast<T>(val);
		return true;
	}
}

template <typename T>
static typename std::enable_if<std::is_same<T, config_switch>::value, bool>::type
assign_str(void* ptr, const char* str) {
	config_switch val = str2switch(str);

	if (val != cfg_inherit) {
		*reinterpret_cast<config_switch*>(ptr) = val;
		return true;
	}
	else return false;
}

template <>
static bool assign_str<bool>(void* ptr, const char* str) {
	config_switch val = str2switch(str);

	if (val == cfg_on) {
		*reinterpret_cast<bool*>(ptr) = true;
	}
	else if (val == cfg_off) {
		*reinterpret_cast<bool*>(ptr) = false;
	}
	else return false;

	return true;
}

template <typename T>
static inline bool assign(T& value, const char* str) {
	return assign_str<T>(reinterpret_cast<void*>(&value), str);
}

template <typename T>
static void print_entry(FILE* f, const char* key, const T& value) {
	if (is_default(value)) return; // Nothing to do if the value is defaulted

	if (std::is_integral<T>::value) {
		if (std::is_same<T, bool>::value) {
			fprintf(f, "%s=%s\n", key, value ? "on" : "off");
		}
		else if (std::is_signed<T>::value) {
			fprintf(f, "%s=%lld\n", key, static_cast<int64_t>(value));
		}
		else {
			fprintf(f, "%s=%llu\n", key, static_cast<uint64_t>(value));
		}
	}
	else if (std::is_floating_point<T>::value) {
		fprintf(f, "%s=%g\n", key, static_cast<double>(value));
	}
	else if (std::is_same<T, config_switch>::value) {
		fprintf(f, "%s=%s\n", key, value == cfg_on ? "on" : "off");
	}
}

template <>
static void print_entry(FILE* f, const char* key, const std::string& value) {
	if (!value.empty()) {
		fprintf(f, "%s=%s\n", key, value.c_str());
	}
}

static void print_entry(FILE* f, const char* key, void* ptr, int type_id, asIScriptEngine* engine) {
	switch (type_id) {
	case asTYPEID_BOOL:
		print_entry(f, key, *reinterpret_cast<bool*>(ptr));
		break;
	case asTYPEID_UINT8:
		print_entry(f, key, *reinterpret_cast<uint8_t*>(ptr));
		break;
	case asTYPEID_INT8:
		print_entry(f, key, *reinterpret_cast<int8_t*>(ptr));
		break;
	case asTYPEID_UINT16:
		print_entry(f, key, *reinterpret_cast<uint16_t*>(ptr));
		break;
	case asTYPEID_INT16:
		print_entry(f, key, *reinterpret_cast<int16_t*>(ptr));
		break;
	case asTYPEID_UINT32:
		print_entry(f, key, *reinterpret_cast<uint32_t*>(ptr));
		break;
	case asTYPEID_INT32:
		print_entry(f, key, *reinterpret_cast<int32_t*>(ptr));
		break;
	case asTYPEID_UINT64:
		print_entry(f, key, *reinterpret_cast<uint64_t*>(ptr));
		break;
	case asTYPEID_INT64:
		print_entry(f, key, *reinterpret_cast<int64_t*>(ptr));
		break;
	case asTYPEID_FLOAT:
		print_entry(f, key, *reinterpret_cast<float*>(ptr));
		break;
	case asTYPEID_DOUBLE:
		print_entry(f, key, *reinterpret_cast<double*>(ptr));
		break;
	default:
		{
			const char* type_name = engine->GetTypeDeclaration(type_id);
			if (type_name == nullptr) {
				ERR("Unknown variable type");
			}
			if (strcmp(type_name, "string") == 0) {
				print_entry(f, key, *reinterpret_cast<std::string*>(ptr));
			}
			else {
				ERR("Unsupported config variable type '%s'", type_name);
			}
		}
		break;
	}
}

#pragma endregion

Result<> load_config(const char* file, asIScriptEngine* script_engine) {
	int r = ini_parse(file, config_setting, script_engine);
	if (r >= 0) return Result<>::success;
	//else if (r > 0) {
	//	return Error(Errors::ConfigParseError, std::string("line ") + std::to_string(r));
	//}
	else if (r == -1) {
		return Errors::CannotOpenFile;
	}
	else if (r == -2) {
		return Errors::BadAlloc;
	}
	else {
		return Errors::Unknown;
	}
}

Result<> save_config(const char* file, asIScriptEngine* script_engine) {
	auto maybe = open(file, "w");
	if (!maybe) return maybe.err;
	FILE* stream = maybe;

	fprintf(stream, "; PlatE Settings\n"); // TODO: global game name
	fprintf(stream, "\n[Video]\n");
	print_entry(stream, "width",      global_config.video.width);
	print_entry(stream, "height",     global_config.video.height);
	print_entry(stream, "fullscreen", global_config.video.fullscreen);

	fprintf(stream, "\n[Audio]\n");
	print_entry(stream, "master_volume", global_config.audio.master_volume);
	print_entry(stream, "bgm_volume",    global_config.audio.bgm_volume);
	print_entry(stream, "sfx_volume",    global_config.audio.sfx_volume);
	print_entry(stream, "stereo",        global_config.audio.stereo);

	dump_controller_config(stream);

	fprintf(stream, "\n[Script]\n");
	for (const auto& entry : auto_vars) {
		print_entry(stream, entry.first.c_str(), entry.second.ptr, entry.second.type_id, script_engine);
	}

	for (const auto& entry : script_cfg_callbacks) {
		fprintf(stream, "\n[Script_%s]\n", entry.first.c_str());

		asIScriptFunction* callback = entry.second.writer;
		asIScriptContext* ctx = script_engine->RequestContext();

		ctx->Prepare(callback);
		ctx->SetObject(stream);
		int r = ctx->Execute();
		if (r == asEXECUTION_EXCEPTION) {
			ERR("Error in script config writer (function '%s' line %d): %s",
				ctx->GetExceptionFunction()->GetName(),
				ctx->GetExceptionLineNumber(), ctx->GetExceptionString());
		}
		ctx->Unprepare();
		script_engine->ReturnContext(ctx);
	}

	fclose(stream);

	return Result<>::success;
}

static int config_setting(void* user, const char* section, const char* key, const char* value) {
#define SECTION(SECNAME) (strcmp(section, SECNAME) == 0)
#define SPREFIX(PREFIX)  (strncmp(section, PREFIX, sizeof(PREFIX) - 1) == 0)
#define KEY(KEYNAME)     (strcmp(key, KEYNAME) == 0)
#define KPREFIX(PREFIX)  (strncmp(key, PREFIX, sizeof(PREFIX) - 1) == 0)
#define KEYVAL(KEYNAME, VAR) else if KEY(KEYNAME) {assign(VAR, value);}
	if SECTION("Video") {
		if (0) {}
		KEYVAL("width",      global_config.video.width)
		KEYVAL("height",     global_config.video.height)
		KEYVAL("fullscreen", global_config.video.fullscreen)
		else {
			ERR("Unrecognized key for section 'Video': %s", key);
			return 0;
		}
	}
	else if SECTION("Audio") {
		if (0) {}
		KEYVAL("master_volume", global_config.audio.master_volume)
		KEYVAL("bgm_volume", global_config.audio.bgm_volume)
		KEYVAL("sfx_volume", global_config.audio.sfx_volume)
		KEYVAL("stereo", global_config.audio.stereo)
		else {
			ERR("Unrecognized key for section 'Audio': %s", key);
			return 0;
		}
	}
	else if SPREFIX("Input_") {
		bind_from_ini(section + 6, key, value);
	}
	else if SECTION("Script") {
		const cstr_key key(key);

		AutoVars::const_iterator iter = auto_vars.find(key);
		if (iter == auto_vars.end()) return 0;

		auto& var = iter->second;

		switch (var.type_id) {
		case asTYPEID_BOOL:
			return assign_str<bool>(var.ptr, value);

		case asTYPEID_INT8:
			return assign_str<int8_t>(var.ptr, value);
		case asTYPEID_UINT8:
			return assign_str<uint8_t>(var.ptr, value);

		case asTYPEID_INT16:
			return assign_str<int16_t>(var.ptr, value);
		case asTYPEID_UINT16:
			return assign_str<uint16_t>(var.ptr, value);

		case asTYPEID_INT32:
			return assign_str<int32_t>(var.ptr, value);
		case asTYPEID_UINT32:
			return assign_str<uint32_t>(var.ptr, value);

		case asTYPEID_INT64:
			return assign_str<int64_t>(var.ptr, value);
		case asTYPEID_UINT64:
			return assign_str<uint64_t>(var.ptr, value);

		default:
			{
				asIScriptEngine* engine = reinterpret_cast<asIScriptEngine*>(user);
				const char* type_name = engine->GetTypeDeclaration(var.type_id);
				if (type_name == nullptr) {
					ERR("Unknown variable type");
					return 0;
				}
				if (strcmp(type_name, "string") == 0) {
					*reinterpret_cast<std::string*>(var.ptr) = value;
					return 1;
				}
				else {
					ERR("Unsupported config variable type '%s'", type_name);
					return 0;
				}
			}
		}
	}
	else if SPREFIX("Script_") {
		const cstr_key script_sec(section + 7);

		SectionCallbacks::const_iterator iter = script_cfg_callbacks.find(script_sec);
		if (iter == script_cfg_callbacks.end()) return 0;

		asIScriptFunction* callback = iter->second.reader;
		asIScriptEngine* engine = reinterpret_cast<asIScriptEngine*>(user);
		asIScriptContext* ctx = engine->RequestContext();

		std::string script_key(key), script_value(value);

		ctx->Prepare(callback);
		ctx->SetArgObject(0, &script_key);
		ctx->SetArgObject(1, &script_value);
		int r = ctx->Execute();
		if (r == asEXECUTION_EXCEPTION) {
			ERR("Error in script config reader (function '%s' line %d): %s", ctx->GetExceptionFunction()->GetName(),
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
#undef KEYVAL
}

void addAutoVar(asIScriptModule* module, asUINT index) {
	const char* name;
	int type_id;
	int r = module->GetGlobalVar(index, &name, nullptr, &type_id);
	if (r < 0) {
		ERR("Failed to add auto config variable\n");
		return;
	}
	void* addr = module->GetAddressOfGlobalVar(index);
	auto_vars.insert(std::make_pair(cstr_key(name, true), config_var{ addr, type_id }));
}

// Kind of overkill- should probably be replaced with a DFA implementation at some point.
static const std::regex ON_PATTERN("on|1|t(rue)?|y(es)?|enabled");
static const std::regex OFF_PATTERN("off|0|f(alse)?|no?|disabled");
static config_switch str2switch(const char* str) {
	char buf[16];
	strncpy(buf, str, 15);
	buf[15] = 0;
	strlwr(buf);
	if (std::regex_match(str, ON_PATTERN)) return cfg_on;
	if (std::regex_match(str, OFF_PATTERN)) return cfg_off;
	else return cfg_inherit;
}

#pragma region Script Interface

static void AddSectionHandler(std::string& section, asIScriptFunction* reader, asIScriptFunction* writer) {
	script_cfg_callbacks.insert(std::make_pair(cstr_key(section.c_str(), true), callback_pair{reader, writer}));
}

static void ReloadConfig() {
	asIScriptContext* ctx = asGetActiveContext();
	auto res = load_config("settings.ini", ctx->GetEngine());
	if (!res) {
		ForwardErrorAsScriptException(ctx, res.err);
	}
}

static void SaveConfig() {
	asIScriptContext* ctx = asGetActiveContext();
	auto res = save_config("settings.ini", ctx->GetEngine());
	if (!res) {
		ForwardErrorAsScriptException(ctx, res.err);
	}
}

static void WriteConfigValue(FILE* f, std::string& key, void* ptr, int type_id) {
	print_entry(f, key.c_str(), ptr, type_id, asGetActiveContext()->GetEngine());
}

#define check(EXPR) do {int r = (EXPR); assert(r >= 0);} while(0)
void RegisterConfigInterface(asIScriptEngine* engine) {
	check(engine->SetDefaultNamespace("Config"));

	check(engine->RegisterObjectType("Writer", 0, asOBJ_REF | asOBJ_NOCOUNT));
	check(engine->RegisterObjectMethod("Writer", "void write(string &in, ?&in)",
		asFUNCTION(WriteConfigValue), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterFuncdef("void ConfigReadCallback(string &in key, string &in value)"));
	check(engine->RegisterFuncdef("void ConfigWriteCallback(Writer@)"));

	check(engine->RegisterGlobalFunction("void addSectionHandlers(string &in, ConfigReadCallback@, ConfigWriteCallback@)",
		asFUNCTION(AddSectionHandler), asCALL_CDECL));

	check(engine->RegisterGlobalFunction("void load()", asFUNCTION(ReloadConfig), asCALL_CDECL));
	check(engine->RegisterGlobalFunction("void save()", asFUNCTION(SaveConfig), asCALL_CDECL));

	check(engine->SetDefaultNamespace(""));
}

#pragma endregion