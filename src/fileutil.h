#pragma once

#include <cstdio>
#include "mempool.h"
#include "arrays.h"
#include "result.h"
#include "error.h"
#include "util.h"
#include "hitbox.h"
#include <SDL2\SDL_endian.h>
#include <type_traits>
#include <cassert>

namespace Errors {
	const error_data
		CannotOpenFile = { 1, "File could not be opened." },
		IncompleteFileRead = { 2, "Did not get all the bytes expected from an fread call." };
}

template <class T, bool checked = true>
T read(FILE* stream) {
	static_assert(std::is_pod<T>::value, "read<T>(FILE*) can only be used for POD types");
	alignas(T) unsigned char buffer[sizeof(T)];

	if (!fread(buffer, sizeof(T), 1, stream) && checked) {
		throw Error(Errors::IncompleteFileRead, typeid(T).name()); // one of the few exceptions actually used in the codebase.
	}

	// V - TODO? - V
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	// reverse buffer
	// reflection?...
#endif

	return *reinterpret_cast<T*>(buffer);
}

Result<FILE*> open(const char* file, const char* mode);
size_t size(FILE* f);
char* read_all(FILE* f);

Hitbox read_hitbox(FILE* stream, MemoryPool& pool);
Array<const Collider> read_colliders(FILE* stream, uint32_t n_colliders, MemoryPool& pool);
GPU_Image* read_referenced_texture(FILE* stream, uint32_t filenamelen, const DirContext& context);
const char* read_string(FILE* stream, unsigned int len, MemoryPool& pool);
const char* read_string(FILE* stream, unsigned int len, char* buffer);
const char* read_string(FILE* stream, unsigned int len);

template <class Header>
inline const char* read_string(FILE* stream) {
	static_assert(std::is_integral<Header>::value, "read_string<Header> is only compatible with int types");
	return read_string(stream, read<Header>(stream));
}

template <class Header>
inline const char* read_string(FILE* stream, char* buffer) {
	static_assert(std::is_integral<Header>::value, "read_string<Header> is only compatible with int types");
	return read_string(stream, read<Header>(stream), buffer);
}

#define check_header(stream, expected) __check_header__(stream, expected, sizeof(expected) - 1)
inline bool __check_header__(FILE* stream, const char* expected, size_t len) {
	assert(len > 0 && len < 32);
	char headbytes[32];
	if (fread(headbytes, 1, len, stream) != len) return false;
	headbytes[len] = 0;
	return strcmp(headbytes, expected) == 0;
}

GPU_Image* load_texture(const char* texname);