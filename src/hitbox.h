#ifndef PLATE_HITBOX_H
#define PLATE_HITBOX_H

#include <cstdint>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include "direction.h"
#include "either.h"
#include "vectors.h"
#include "storage.h"
#include "transform.h"

struct Hitbox {
	enum : char {
		BOX,	   // Sprite-aligned bounding box
		CIRCLE,    // Hit-bubble
		LINE,      // Line
		ONEWAY,    // Line that only collides from one side. Only works for solid collisions
		           //   Solid side is the top side for left-to-right horizontal lines
		POLYGON,   // Convex polygon
		COMPOSITE  // A combination of hitboxes
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
	Hitbox(Hitbox& other);
};

struct CollisionType {
	const char* name;
	int id;
	SDL_Color color;

private:
	static Array2D<bool> table;
	static Array<const CollisionType> types;

public:
	static void init (const char* config_file);
	inline static bool acts_on(const CollisionType* a, const CollisionType* b) {
		if (a == nullptr || b == nullptr) {
			return false;
		}
		return table(a->id, b->id);
	}
	static const CollisionType* by_name (const char* name);

private:
	inline CollisionType(const char* name, int id, SDL_Color color) :
		name(name), id(id), color(color) {}

	inline CollisionType() {}
};

struct Collider {
	/// type data that determines what things actually collide
	const CollisionType* type;

	/// The inherently meaningful data that the engine handles.
	Hitbox hitbox;

	/// Solidity ensures that physics depenetrates entities
	bool solid : 1;

	/// Is continuous collision detection enabled
	/// CCD prevents fast-moving objects from tunneling, but is more expensive to calculate
	bool ccd : 1;

	// priority?
};

typedef Array<const Collider> CollisionData;

void render_colliders(SDL_Renderer* context, const Transform& tx, const CollisionData& colliders);

// Collision detection :O
bool hitboxes_overlap(
	const Hitbox* a, const Transform& aTx, Vector2 aDis,
	const Hitbox* b, const Transform& bTx, Vector2 bDis
);

#endif