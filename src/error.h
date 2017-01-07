#pragma once

#include <cstdio>
#include <string>
#include "angelscript.h"

// Debug/Reporting Verbosity Levels
#define LOGLEVEL_VERBOSE 10
#define LOGLEVEL_DEBUG 8
#define LOGLEVEL_NORMAL 5
#define LOGLEVEL_RELEASE 1
#define LOGLEVEL_SILENT 0
// This probably should be moved to config files :-/
#define LOG_VERBOSITY LOGLEVEL_VERBOSE

#if LOG_VERBOSITY >= LOGLEVEL_VERBOSE
#define LOG_VERBOSE(fmt, ...) printf(fmt, __VA_ARGS__)
#define ERR_VERBOSE(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define LOG_VERBOSE(fmt, ...)
#define ERR_VERBOSE(fmt, ...)
#endif

#if LOG_VERBOSITY >= LOGLEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) printf(fmt, __VA_ARGS__)
#define ERR_DEBUG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#define ERR_DEBUG(fmt, ...)
#endif

#if LOG_VERBOSITY >= LOGLEVEL_NORMAL
#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)
#define ERR(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define LOG(fmt, ...)
#define ERR(fmt, ...)
#endif

#if LOG_VERBOSITY >= LOGLEVEL_RELEASE
#define LOG_RELEASE(fmt, ...) printf(fmt, __VA_ARGS__)
#define ERR_RELEASE(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define LOG_RELEASE(fmt, ...)
#define LOG_RELEASE(fmt, ...)
#endif

namespace Errors {
	struct error_data {
		int code;
		const char* description;
	};

	const error_data
		BadAlloc = { -9999, "Bad Allocation!" };
}

// Code ranges
// 0: reserved for "nothing wrong" (alternatively, you can use nullptr)
// negative: Errors that originated from standard libraries, external libraries (e.g. SDL), or the OS
//   -- that is errors that neither the engine nor content creator has control over
// 1-199: Generic errors that occur in many parts of the code
// 200-299: Sprite related errors
// 300-499: Entity related errors
// 500-599: Event related errors
// 600-699: Level related errors
// 2000-2199: Scripting related errors

//typedef Errors::error_data Error;
struct Error {
	int code;
	const char* description;
	std::string details;

	Error(): details() {}

	__forceinline Error(const Errors::error_data& edata) :
		code(edata.code), description(edata.description), details()
	{}

	__forceinline Error(const Errors::error_data& edata, std::string&& str) :
		code(edata.code), description(edata.description), details(std::move(str))
	{}

	__forceinline Error(const Errors::error_data& edata, const std::string& str) :
		code(edata.code), description(edata.description), details(str)
	{}

	Error(Error&& other) = default;
	Error(const Error& other) = default;

	inline void operator = (Error&& other) {
		code = other.code;
		description = other.description;
		details = std::move(other.details);
	}

	inline void operator = (const Error& other) {
		code = other.code;
		description = other.description;
		details = other.details;
	}

	inline void operator = (const Errors::error_data& edata) {
		code = edata.code;
		description = edata.description;
		details = std::string();
	}
};

namespace std {
	std::string to_string(const Error& err);
}

// AngelScript-interfacing utility functions

void DispatchErrorCallback(asIScriptFunction* callback, const Error& err);

std::string GetExceptionDetails(asIScriptContext* ctx);