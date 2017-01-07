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

namespace Errors {
	const error_data
		InvalidHitboxType = { 190, "Hitbox type is invalid." };
}

struct Hitbox {
	enum Type : char {
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
	Hitbox(Hitbox&& other);

	void operator = (const Hitbox& other);
	void operator == (Hitbox&& other);
};

struct ColliderGroup {
	const char* name;
	int id;
	SDL_Color color;

private:
	static Array2D<bool> table;
	static Array<const ColliderGroup> types;

public:
	static void init (const char* config_file);
	inline static bool acts_on(const ColliderGroup* a, const ColliderGroup* b) {
		if (a == nullptr || b == nullptr) {
			return false;
		}
		return table(a->id, b->id);
	}
	static const ColliderGroup* by_name (const char* name);

private:
	inline ColliderGroup(const char* name, int id, SDL_Color color) :
		name(name), id(id), color(color) {}

	inline ColliderGroup() {}
};

struct Collider {
	/// type data that determines what things actually collide
	const ColliderGroup* type;

	/// The inherently meaningful data that the engine handles.
	Hitbox hitbox;

	// priority?
};

void render_hitbox(GPU_Target* context, const Transform& tx, const Hitbox& hitbox, const SDL_Color& color);
void render_colliders(GPU_Target* context, const Transform& tx, const Array<const Collider>& colliders);

// Collision detection :O
bool hitboxes_overlap(
	const Hitbox* a, const Transform& aTx, Vector2 aDis,
	const Hitbox* b, const Transform& bTx, Vector2 bDis
);

#endif