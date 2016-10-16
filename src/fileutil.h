#pragma once

#include <cstdio>
#include "either.h"
#include "error.h"
#include "macros.h"

namespace Errors {
	const error_data
		IncompleteFileRead = {2, "Did not get all the bytes expected from an fread call."};
}

template <class T, bool checked = true>
T read(FILE* stream) {
	unsigned char buffer[sizeof(T)];
	// TODO? Endianness
	// #if BYTE_ORDER == BIG_ENDIAN
	// reverse buffer
	// #endif

	size_t bytes_read = fread(buffer, 1, sizeof(T), stream);
	if (checked && bytes_read != sizeof(T)) {
		throw Errors::IncompleteFileRead;
	}

	return *reinterpret_cast<T*>(buffer);
}
