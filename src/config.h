#pragma once

#include "angelscript.h"
#include "result.h"
#include "error.h"

namespace Errors {
	const error_data
		ConfigParseError = { 30, "Error in parsing config file" };
}

enum config_switch : int8_t {
	cfg_off, cfg_on, cfg_inherit = -1
};

template<typename T>
constexpr T config_default();

template<>
constexpr uint8_t  config_default() { return static_cast<uint8_t>(0xFF); }
template<>
constexpr uint16_t config_default() { return static_cast<uint16_t>(0xFFFF);}
template<>
constexpr uint32_t config_default() { return 0xFFFFffff; }
template<>
constexpr uint64_t config_default() { return 0xFFFFffffFFFFffff; }
template<>
constexpr int8_t   config_default() { return static_cast<uint8_t>(0x80); }
template<>
constexpr int16_t  config_default() { return static_cast<uint16_t>(0x8000); }
template<>
constexpr int32_t  config_default() { return 0x80000000; }
template<>
constexpr int64_t  config_default() { return 0x8000000000000000; }
template<>
constexpr float    config_default() { return std::numeric_limits<float>::quiet_NaN(); }
template<>
constexpr double   config_default() { return std::numeric_limits<double>::quiet_NaN(); }
template<>
constexpr bool config_default() { return false; }
template<>
constexpr config_switch config_default() { return cfg_inherit; }

template<typename T>
__forceinline bool is_default(T val) {return (val == config_default<T>());}

#define CFG_FIELD(NAME, TYPE) TYPE NAME = config_default<TYPE>();

struct Configuration {
	struct Video {
		CFG_FIELD(width, uint16_t)
		CFG_FIELD(height, uint16_t)
		CFG_FIELD(fullscreen, config_switch)
	} video;

	struct Audio {
		CFG_FIELD(master_volume, uint8_t)
		CFG_FIELD(bgm_volume, uint8_t)
		CFG_FIELD(sfx_volume, uint8_t)
		CFG_FIELD(stereo, config_switch)
	} audio;
};

Result<> load_config(const char* file, asIScriptEngine* script_engine);

Result<> save_config(const char* file, asIScriptEngine* script_engine);

Configuration& get_global_config();

void addAutoVar(asIScriptModule* module, asUINT index);

void RegisterConfigInterface(asIScriptEngine* engine);

