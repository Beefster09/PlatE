#pragma once

#include <cstdio>
#include "either.h"
#include "error.h"
#include "macros.h"
#include "hitbox.h"
#include <SDL2\SDL_endian.h>
#include <type_traits>

namespace Errors {
	const error_data
		IncompleteFileRead = {2, "Did not get all the bytes expected from an fread call."};
}

template <class T, bool checked = true>
T read(FILE* stream) {
	unsigned char buffer[sizeof(T)];

	// TODO? Endianness

	size_t bytes_read = fread(buffer, 1, sizeof(T), stream);
	if (checked && bytes_read != sizeof(T)) {
		throw Errors::IncompleteFileRead;
	}

	// V - TODO? - V
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	// reverse buffer
	// reflection?...
#endif

	return *reinterpret_cast<T*>(buffer);
}

Hitbox read_hitbox(FILE* stream, MemoryPool& pool);