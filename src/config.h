#pragma once

#include "angelscript.h"
#include "result.h"

constexpr uint8_t  U8_DEFAULT  = 0xFF;
constexpr uint16_t U16_DEFAULT = 0xFFFF;
constexpr uint32_t U32_DEFAULT = 0xFFFFffff;
constexpr uint64_t U64_DEFAULT = 0xFFFFffffFFFFffff;
constexpr int8_t   I8_DEFAULT  = 0x80;
constexpr int16_t  I16_DEFAULT = 0x8000;
constexpr int32_t  I32_DEFAULT = 0x80000000;
constexpr int64_t  I64_DEFAULT = 0x8000000000000000;
constexpr float  FLOAT_DEFAULT = std::numeric_limits<float>::quiet_NaN();

struct Configuration {
	enum config_switch : int8_t {
		off, on, inherit = -1
	};

	struct Video {
		uint16_t resolution_x = U16_DEFAULT;
		uint16_t resolution_y = U16_DEFAULT;
		config_switch fullscreen = inherit;
	} video;

	struct Audio {
		uint8_t master_volume = U8_DEFAULT;
		uint8_t bgm_volume = U8_DEFAULT;
		uint8_t sfx_volume = U8_DEFAULT;
		config_switch stereo = inherit;
	} audio;
};

Result<> load_config(const char* file, asIScriptEngine* script_engine);

Result<> save_config(const char* file, asIScriptEngine* script_engine);

Configuration& get_global_config();

void RegisterConfigInterface(asIScriptEngine* engine);