#pragma once

#include <cstdio>

namespace Errors {
	typedef struct {
		int code;
		const char* description;
	} error_data;

	const error_data
		FileNotFound = { 1, "File not found" },
		InvalidJson = { 10, "Json is not well-formed" };
}

// Code ranges
// 0: reserved for "nothing wrong" (alternatively, you can use nullptr)
// negative: Errors that originated from external libraries (e.g. SDL) or the OS
//   -- that is errors that neither the engine nor content creator has control over
// 1-199: Generic errors that occur in many parts of the code
// 200-299: Sprite related errors
// 300-499: Entity related errors

typedef Errors::error_data Error;

const Error SUCCESS = { 0, "Nothing is wrong" }; // IRONIC SENTINEL VALUE

inline Error DetailedError(const Error& err, const char* format, ...) {
	va_list(args);
	va_start(args, format);
	vprintf(format, args);
	return err;
}
