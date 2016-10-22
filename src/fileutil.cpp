
#include "storage.h"
#include "fileutil.h"
#include "error.h"
#include "hitbox.h"
#include <cstdio>
#include <cerrno>

Hitbox read_hitbox(FILE* stream, MemoryPool& pool) {
	Hitbox result;

	result.type = read<Hitbox::Type>(stream);
	switch (result.type) {
	case Hitbox::BOX:
		result.box = read<AABB>(stream);
		break;
	case Hitbox::CIRCLE:
		result.circle = read<Circle>(stream);
		break;
	case Hitbox::LINE:
	case Hitbox::ONEWAY:
		result.line = read<Line>(stream);
		break;
	case Hitbox::POLYGON:
	{
		size_t n_vertices = read<uint32_t>(stream);
		Vector2* vertices = pool.alloc<Vector2>(n_vertices);
		for (int i = 0; i < n_vertices; ++i) {
			vertices[i] = read<Vector2>(stream);
		}
		result.polygon.vertices = Array<const Vector2>(vertices, n_vertices);
		//result.polygon.aabb = get_aabb(result.polygon.vertices);
		break;
	}
	case Hitbox::COMPOSITE:
	{
		size_t n_subs = read<uint32_t>(stream);
		Hitbox* subs = pool.alloc<Hitbox>(n_subs);
		for (int i = 0; i < n_subs; ++i) {
			subs[i] = read_hitbox(stream, pool);
		}
		result.composite.hitboxes = Array<const Hitbox>(subs, n_subs);
		break;
	}
	default:
		throw Errors::InvalidHitboxType;
	}

	return result;
}

Array<const Collider> read_colliders(FILE* stream, uint32_t n_colliders, MemoryPool& pool) {
	Collider* colliders = pool.alloc<Collider>(n_colliders);
	for (int i = 0; i < n_colliders; ++i) {
		Collider& cur_coll = colliders[i];

		size_t typestrlen = read<uint32_t>(stream);
		cur_coll.solid = read<bool>(stream);
		cur_coll.ccd = read<bool>(stream);

		char namebuffer[51];
		fread(namebuffer, 1, typestrlen, stream);
		namebuffer[typestrlen] = 0;
		cur_coll.type = CollisionType::by_name(namebuffer);

		cur_coll.hitbox = read_hitbox(stream, pool);
	}
	return Array<const Collider>(colliders, n_colliders);
}

Either<Error, FILE*> open(const char* filename, const char* mode) {
	FILE* file = fopen(filename, mode);

	if (file == nullptr) {
		// TODO: more descriptive errors...
		return Errors::CannotOpenFile;
	}

	return file;
}

size_t size(FILE* f) {
	size_t orig_pos = ftell(f);

	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	fseek(f, orig_pos, SEEK_SET);

	return len;
}

char* read_all(FILE* f) {
	size_t len = size(f);
	char* buffer = new char[len + 1];
	size_t bytes_read = fread(buffer, 1, len, f);
	buffer[bytes_read] = 0;
	return buffer;
}

const char* read_string(FILE* stream, unsigned int len, MemoryPool& pool) {
	char* str = pool.alloc<char>(len + 1);

	if (str == nullptr) return nullptr; // Memory Pool is full

	if (fread(str, 1, len, stream) != len) return nullptr;

	str[len] = 0;
	return str;
}