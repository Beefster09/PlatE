#pragma once

#include <cstdint>
#include <cmath>
#include <cassert>
#include "sprite.h"
#include "hitbox.h"
#include "either.h"
#include "event.h"
#include "vectors.h"

#define ENTITY_SYSTEM_DEFAULT_SIZE 256

namespace Errors {
	const error_data
		EntitySystemInvalidSize = { 300, "Entity system must have at least 1 max entity" },
		EntitySystemCapacityReached = { 301, "Entity system has reached its maximum entity count" },
		EntitySystemInvalidState = { 499, "Entity system has been corrupted or tampered with!" };
}

// Classes of entities. Should be const when loaded.
// Contains metadata about behavior and initialization.
struct EntityClass {
	const char* name;
	const Sprite* initial_sprite;
	size_t initial_animation;

	bool self_colliding: 1; // instances of this entity class collide with other instances of the same class

	//  \- BEHAVIOR -/
	// void* init_script
	// void* update_script
	// void* event_handlers
};

typedef uint32_t EntityId;

// Instances of entities
struct Entity {
	EntityId id;
	const EntityClass* e_class;

	Point2 position;

	Vector2 velocity;
	Vector2 acceleration;

	// Needed for tunnelling prevention and effects such as motion blur.
	Point2 last_pos;
	float last_dt;

	// Other movement stuffs
	Vector2 max_speed; // Maximum speed on each axis
	Vector2 gravity;   // Additional acceleration to apply when in midair

	bool onGround; // This will probably change type to a pointer later

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
};

// Things to think about:
//  * How is this supposed to interact with players?
//    - probably scripts
//  * Should particles and bullets be managed by an entity systems?
//  * Should they be fixed-size or expandable?
struct EntitySystem {
	// Sparse allocation
	Entity* entities;
	bool* has_entity; // TODO: turn into dynamic bitarray that isn't a vector<bool>

	// Densely allocated
	Entity** z_ordered_entities;

	EventBuffer* event_buffer;

	EntityId next_id;
	size_t next_index;
	size_t n_entities;
	size_t max_entities;
};

Either<Error, EntitySystem*> create_entity_system(size_t capacity = ENTITY_SYSTEM_DEFAULT_SIZE);
Error dispose_entity_system(EntitySystem* system);

Either<Error, Entity*> spawn_entity(EntitySystem* system, const EntityClass* e_class, Point2 position);
Error destroy_entity(EntitySystem* system, EntityId id);

// TODO/later: allow interactions between entity systems and level collision data
void process_entities(int delta_time, EntitySystem* system);

// self-explanatory
void render_entities(SDL_Renderer* context, const EntitySystem* system);