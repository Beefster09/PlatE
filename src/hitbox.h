#ifndef PLATE_HITBOX_H
#define PLATE_HITBOX_H

#include <cstdint>

typedef enum {
	BOX = 1,   // Axis-aligned bounding box
	LINE,
	CIRCLE
} HitboxShape;

typedef enum {
	SOLID = 0,  // Collides with other solid hitboxes
	ONEWAY,     // Collides with solid hitboxes from one side
	HURTBOX,    // Reports events when hit by damage hitboxes
	BLOCK,		// Disables damage hitboxes that touch this. Takes priority over hurtboxes. Might not work from all sides.
	DAMAGE,     // Reports an event when hitting a hurtbox or block, then stops colliding its group until refreshed
	TRIGGER,	// Reports an event when initially colliding with a solid hitbox
	AREA        // Reports events when solid hitboxes overlap at least 50% with this
} HitboxType;

const uint32_t HITBOX_COLORS[] = {
	0xC0C0C0,
	0x808080,
	0xEEEE00,
	0x6000C0,
	0xFF0000,
	0x000000,
	0x000000
};

// Indices for various data values
#define HITBOX_ONEWAY_DATA_DIRECTION 0
#define HITBOX_BLOCK_DATA_DIRECTION 0
#define HITBOX_DAMAGE_DATA_GROUP 0
#define HITBOX_DAMAGE_DATA_PRIORITY 1
#define HITBOX_AREA_DATA_PRIORITY 0

typedef struct {
	HitboxShape shape;
	union {
		struct {
			short x, y, h, w;
		} box;
		struct {
			short x1, y1, x2, y2;
		} line;
		struct {
			short x, y;
			float radius;
		} circle;
	};
	HitboxType type;
	int flags; // Only collides hitboxes with at least one flag matching
	int data[4]; // Data specific to each HitboxType
} Hitbox;

#endif