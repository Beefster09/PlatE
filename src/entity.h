#pragma once

#include <cstdint>
#include <cmath>
#include <cassert>
#include "sprite.h"
#include "hitbox.h"
#include "either.h"
#include "event.h"
#include "vectors.h"
#include "storage.h"
#include "transform.h"

#define ENTITY_SYSTEM_DEFAULT_SIZE 256

namespace Errors {
	const error_data
		EntitySystemInvalidSize = { 300, "Entity system must have at least 1 max entity" },
		EntitySystemCapacityReached = { 301, "Entity system has reached its maximum entity count" },
		EntitySystemInvalidState = { 499, "Entity system has been corrupted or tampered with!" },
		NoSuchEntity = { 310, "Entity does not exist in this system!" };
}

// Classes of entities. Should be const when loaded.
// Contains metadata about behavior and initialization.
struct EntityClass {
	const char* name;
	const Sprite* initial_sprite;
	size_t initial_animation;

	bool self_colliding: 1; // instances of this entity class collide with other instances of the same class

	//  \- BEHAVIOR -/
	// Script* init_script;
	// Script* update_script;
	// Script* event_handlers;
	// Array<Parameter> spawn_params;
};

typedef uint32_t EntityId;

// Instances of entities
struct Entity {
// === Metadata ===
	EntityId id;
	const EntityClass* e_class;

// === Physics Data ===
	Point2 position;

	Vector2 velocity;
	Vector2 acceleration;

	// Needed for tunnelling prevention and effects such as motion blur.
	Point2 last_pos;
	float last_dt;

	// Other movement stuffs
	Vector2 max_speed; // Maximum speed on each axis
	Vector2 gravity;   // Additional acceleration to apply when in midair

	struct GroundLink {
		enum : char {
			MIDAIR = 'm',
			ENTITY = 'e',
			LEVEL = 'l'
		} type;
		union {
			Entity* entity;
			void* level;
		};
		// Where this entity's foot position is relative to the linked origin.
		Vector2 foot_pos;
	} ground;

	int z_order; // needed for drawing

// === Transform Data ===
	float rotation;
	Vector2 scale;

	// Cached transformation matrix. Calculated during physics/movement step and after update.
	Transform transform_matrix;

// === Sprite Data ===
	const Sprite* sprite;
	const Animation* curAnimation;
	uint32_t curAnimFrame;
	float curFrameTime;
	const Frame* curFrame;

	// Scripting / Events
	// Data for storing
};

// Things to think about:
//  * How is this supposed to interact with players?
//    - probably scripts
//  * Should particles and bullets be managed by an entity system?
//  * Should they be fixed-size or expandable?
struct EntitySystem {
	SparseBucket<Entity> entities;

	// Densely allocated
	Entity** z_ordered_entities;

	EventBuffer* event_buffer;

	uint32_t next_id;
};

Either<Error, EntitySystem*> create_entity_system(size_t capacity = ENTITY_SYSTEM_DEFAULT_SIZE);
Error dispose_entity_system(EntitySystem* system);

Either<Error, Entity*> spawn_entity(EntitySystem* system, const EntityClass* e_class, Point2 position);
Option<Error> destroy_entity(EntitySystem* system, EntityId id);
Option<Error> destroy_entity(EntitySystem* system, Entity* ent);

// TODO/later: allow interactions between entity systems and level collision data
void process_entities(int delta_time, EntitySystem* system);

// self-explanatory
void render_entities(SDL_Renderer* context, const EntitySystem* system);