#ifndef PLATE_HITBOX_H
#define PLATE_HITBOX_H

#include <cstdint>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_pixels.h>
#include "direction.h"
#include "either.h"
#include "vectors.h"

struct Hitbox {
	enum : char {
		BOX,	// Sprite-aligned bounding box
		CIRCLE, // Hit-bubble
		LINE,   // Line
		ONEWAY  // Line that only collides from one side. Only works for solid collisions
		        //   Solid side is the top side for left-to-right horizontal lines
		// PIXELPERFECT ?
	} shape;

	union {
		SDL_Rect box;
		struct {
			int x1, y1, x2, y2;
		} line;
		struct {
			int x1, y1, x2, y2;
		} oneway;
		struct {
			int x, y;
			float radius;
		} circle;
	};
};

enum class HitboxType {
	UNKNOWN,  // because reasons
	SOLID,        // Collides with other solid hitboxes
	TRIGGER,	  // Reports an event when initially colliding with a solid hitbox
	HURTBOX,      // Reports events when hit by damage hitboxes
	BLOCK,		  // Disables damage hitboxes that touch this. Takes priority over hurtboxes. Might not work from all sides.
	DAMAGE,       // Reports an event when hitting a hurtbox or block, then stops colliding its group until refreshed
	AREA          // Reports events when solid hitboxes overlap at least 50% with this
};

struct HitboxGroup {
	HitboxType type; // determines what this collides with and what data is passed in events
	union {
		struct {} solid;
		struct {} trigger;
		struct {
			int priority;
		} hurtbox;
		struct {
			int priority;
		} block;
		struct {
			int priority;
		} damage;
		struct {
			int priority;
			float minimum_containing;
		} area;
	};

	uint64_t flags;
	// HitboxGroups will only collide if their types are compatible and at least one flag matches
	// I'm not exactly sure how this will look on the user-facing side...

	int n_hitboxes;
	const Hitbox* hitboxes;
};

struct CollisionData {
	float radius_squared;    // Used for sweep and prune checks

	int n_groups;
	const HitboxGroup* groups;
};

HitboxType hitbox_type_by_name(const char* name);
const char* name_of(HitboxType type);
SDL_Color get_hitbox_color(HitboxType type, int flags = 0);
bool hitbox_types_collide(HitboxType a, HitboxType b);
bool hitbox_acts_on (HitboxType a, HitboxType b); // Returns true if a and b are in the correct order

void get_hitbox_rects_relative_to(SDL_Rect* rects, const HitboxGroup& hitboxes, SDL_Point origin);

// Collision detection :O
bool hitboxes_overlap(const Hitbox* a, int ax, int ay, const Hitbox* b, int bx, int by);

#endif