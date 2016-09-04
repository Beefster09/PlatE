#pragma once

namespace Errors {
	typedef const struct {
		int code;
		const char* description;
	} error_data;

	error_data
		FileNotFound = { 1, "File not found" },
		InvalidJson = { 10, "Json is not well-formed" };
}

// Code ranges
// 0: reserved for "nothing wrong" (alternatively, you can use nullptr)
// negative: Errors that originated from external libraries (e.g. SDL)
// 1-199: Generic errors that occur in many parts of the code
// 200-299: Sprite related errors
// 300-499: Entity related errors

typedef Errors::error_data Error;

Error SUCCESS = { 0, "Nothing is wrong" }; // IRONIC SENTINEL VALUE
