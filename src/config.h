#pragma once

#include "ini.h"
#include "angelscript.h"

void load_config(const char* file);

void save_config(const char* file);

void RegisterConfigInterface(asIScriptEngine* engine);