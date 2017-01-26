#ifndef PLATE_HITBOX_H
#define PLATE_HITBOX_H

#include <cstdint>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include "direction.h"
#include "result.h"
#include "vectors.h"
#include "storage.h"
#include "transform.h"
#include "SDL_gpu.h"
#include <cstdio>

namespace Errors {
	const error_data
		InvalidHitboxType = { 190, "Hitbox type is invalid." };
}

struct Hitbox {
	enum Type : char {
		NONE      =  0,  // No hitbox
		BOX       = 'b', // Sprite-aligned bounding box
		CIRCLE    = 'c', // Hit-bubble
		LINE      = 'l', // Line
		ONEWAY    = 'o', // Line that only collides from one side. Only works for solid collisions
		                 //   Solid side is the top side for left-to-right horizontal lines
		POLYGON   = 'p', // Convex polygon
		COMPOSITE = '?'  // A combination of hitboxes
	} type;

	union {
		AABB box;
		Line line;
		Circle circle;
		struct {
			Array<const Point2> vertices;
			AABB aabb;
		} polygon;
		struct {
			Array<const Hitbox> hitboxes;
			AABB aabb;
		} composite;
	};

	Hitbox() {}
	Hitbox(const Hitbox& other);
	void operator = (const Hitbox& other);
};

struct ColliderType {
	const char* name;
	int id;
	SDL_Color color;

private:
	static Array2D<bool> table;
	static Array<const ColliderType> types;

public:
	static Result<> init(FILE* stream);
	inline static bool acts_on(const ColliderType* a, const ColliderType* b) {
		if (a == nullptr || b == nullptr) return false;
		else return table(a->id, b->id);
	}
	static const ColliderType* by_name(const char* name);

//private:
//	inline ColliderType(const char* name, int id, SDL_Color color) :
//		name(name), id(id), color(color) {}
//
//	inline ColliderType() {}
};

struct ColliderChannel {
	const char* name;
	uint8_t id;

private:
	static Array<const ColliderChannel> channels;

public:
	static Result<> init(FILE* stream);
	static const ColliderChannel* by_name(const char* name);
};

struct Collider {
	/// type data that determines what things actually collide
	const ColliderType* type;

	/// The inherently meaningful data that the engine handles.
	Hitbox hitbox;
};

void render_hitbox(GPU_Target* context, const Transform& tx, const Hitbox& hitbox, const SDL_Color& color);
void render_colliders(GPU_Target* context, const Transform& tx, const Array<const Collider>& colliders);

// Collision detection :O
bool hitboxes_overlap(
	const Hitbox& a, const Transform& aTx, Vector2 aDis,
	const Hitbox& b, const Transform& bTx, Vector2 bDis
);

#endif