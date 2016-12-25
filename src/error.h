#pragma once

#include <cstdio>
#include <cstdarg>

namespace Errors {
	typedef struct {
		int code;
		const char* description;
	} error_data;

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

typedef Errors::error_data Error;

const Error SUCCESS = { 0, "Nothing is wrong" }; // IRONIC SENTINEL VALUE

inline Error DetailedError(const Error& err, const char* format, ...) {
	va_list(args);
	va_start(args, format);
	vprintf(format, args);
	return err;
}

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
