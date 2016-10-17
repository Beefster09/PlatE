
#include "storage.h"
#include "fileutil.h"
#include "error.h"
#include "hitbox.h"
#include <cstdio>

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