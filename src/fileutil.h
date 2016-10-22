#pragma once

#include <cstdio>
#include "storage.h"
#include "either.h"
#include "error.h"
#include "macros.h"
#include "hitbox.h"
#include <SDL2\SDL_endian.h>
#include <type_traits>
#include <cassert>

namespace Errors {
	const error_data
		CannotOpenFile = { 1, "File could not be opened ;'(" },
		IncompleteFileRead = { 2, "Did not get all the bytes expected from an fread call." };
}

template <class T, bool checked = true>
T read(FILE* stream) {
	static_assert(std::is_pod<T>::value, "read<T>(FILE*) can only be used for POD types");
	alignas(T) unsigned char buffer[sizeof(T)];

	if (!fread(buffer, sizeof(T), 1, stream) && checked) {
		throw Errors::IncompleteFileRead;
	}

	// V - TODO? - V
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	// reverse buffer
	// reflection?...
#endif

	return *reinterpret_cast<T*>(buffer);
}

Either<Error, FILE*> open(const char* file, const char* mode);
size_t size(FILE* f);
char* read_all(FILE* f);

Hitbox read_hitbox(FILE* stream, MemoryPool& pool);
Array<const Collider> read_colliders(FILE* stream, uint32_t n_colliders, MemoryPool& pool);
const char* read_string(FILE* stream, unsigned int len, MemoryPool& pool);