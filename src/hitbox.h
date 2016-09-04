#ifndef PLATE_HITBOX_H
#define PLATE_HITBOX_H

#include <cstdint>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_pixels.h>

enum class HitboxShape {
	BOX = 1,   // Axis-aligned bounding box
	LINE,
	CIRCLE
};

typedef struct {
	HitboxShape shape;
	union {
		struct {
			short x, y, w, h;
		} box;
		struct {
			short x1, y1, x2, y2;
		} line;
		struct {
			short x, y;
			float radius;
		} circle;
	};
} Hitbox;

enum class HitboxType {
	UNKNOWN = -1,
	SOLID = 0,  // Collides with other solid hitboxes
	ONEWAY,     // Collides with solid hitboxes from one side
	HURTBOX,    // Reports events when hit by damage hitboxes
	BLOCK,		// Disables damage hitboxes that touch this. Takes priority over hurtboxes. Might not work from all sides.
	DAMAGE,     // Reports an event when hitting a hurtbox or block, then stops colliding its group until refreshed
	TRIGGER,	// Reports an event when initially colliding with a solid hitbox
	AREA        // Reports events when solid hitboxes overlap at least 50% with this
};

typedef struct {
	HitboxType type;
	int n_hitboxes;
	const Hitbox* hitboxes;
	uint64_t flags; // Only collides groups with at least one flag matching
	union {
		struct {
			int direction;
		} oneway;
		struct {
			int direction;
		} block;
		struct {
			int priority;
		} damage;
		struct {
			int priority;
		} area;
	};
} HitboxGroup;

HitboxType hitbox_type_by_name(const char* name);
SDL_Color get_hitbox_color(HitboxType type, int flags = 0);
SDL_Rect* get_hitbox_rects_relative_to(const HitboxGroup&, SDL_Point);

#endif