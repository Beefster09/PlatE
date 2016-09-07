#pragma once

#include <cstdint>
#include <cmath>
#include <cassert>
#include "sprite.h"
#include "hitbox.h"
#include "either.h"
#include "pixelunit.h"

#define ENTITY_SYSTEM_DEFAULT_SIZE 256

namespace Errors {
	const error_data
		EntitySystemInvalidSize = { 300, "Entity system must have at least 1 max entity" },
		EntitySystemCapacityReached = { 301, "Entity system has reached its maximum entity count" },
		EntitySystemInvalidState = { 499, "Entity system has been corrupted or tampered with!" };
}

typedef struct pixel_pos {
	PixelUnit x, y;
} PixelPosition;

typedef struct vector2 {
	float x, y;
} Vector2;

struct entity;

// Classes of entities. Should be const when loaded.
// Contains metadata about behavior and initialization.
typedef struct entity_class {
	const char* name;
	const Sprite* initial_sprite;
	size_t initial_animation;

	//  \- BEHAVIOR -/
	// void* init_script
	// void* update_script
	// void* event_handlers
} EntityClass;

// Instances of entities
typedef struct entity {
	uint32_t id;
	const EntityClass* e_class;

	PixelPosition position;

	Vector2 velocity;
	Vector2 acceleration;

	// Needed for tunnelling prevention and effects such as motion blur.
	PixelPosition last_pos;
	float last_dt;

	int z_order; // needed for drawing
	float rotation; // will be needed later

	const Sprite* sprite;
	const Animation* curAnimation;
	uint32_t curAnimFrame;
	float curFrameTime;
	const Frame* curFrame;

	// Scripting / Events
	/*
	int32_t scriptSlots[16];
	*/
} Entity;

typedef struct entity_system {
	// Sparse allocation
	Entity* entities;
	bool* has_entity; // TODO: turn into dynamic bitarray that isn't a vector<bool>

	// Densely allocated
	Entity** z_ordered_entities;

	uint32_t next_id;
	size_t next_index;
	size_t n_entities;
	size_t max_entities;
} EntitySystem;

Either<Error, EntitySystem*> create_entity_system(size_t capacity = ENTITY_SYSTEM_DEFAULT_SIZE);
Error dispose_entity_system(EntitySystem* system);

Either<Error, Entity*> spawn_entity(EntitySystem* system, const EntityClass* e_class, PixelPosition position);
Error destroy_entity(EntitySystem* system, uint32_t id);

// TODO/later: allow interactions between entity systems and level collision data
// Things to think about:
//  * Is this where physics/collision detection are calculated?
//    - Or is it done on another pass?
//  * Are events processed at this time?
//    - Are events synchronous or asynchronous (which would require some sort of queue structure...)
//    - Is the events queue shared or individual?
//  * How is this supposed to interact with players?
void process_entities(int delta_time, EntitySystem* system);

// self-explanatory
void render_entities(SDL_Renderer* context, const EntitySystem* system);