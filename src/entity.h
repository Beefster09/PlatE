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
		EntitySystemCapacityReached = { 300, "Entity system has reached its maximum entity count" };
}

typedef struct entity {
	size_t index;
	uint32_t id;

	struct {
		PixelUnit x, y;
	} position;

	struct {
		float x, y;
	} velocity;

	struct { // Not sure if this is how I should do gravity...
		float x, y;
	} acceleration;

	const Sprite* sprite;
	const Animation* curAnimation;
	uint32_t curAnimFrame;
	float curFrameTime;
	const Frame* curFrame;

	// Scripting / Events
	/*
	void* script; // not sure what type this should be...
	int32_t scriptSlots[16];
	void* callbacks; // event callback struct (LATER)
	*/
} Entity;

typedef struct entity_system {
	Entity* entities;

	uint32_t nextId;
	size_t n_entities;
	size_t max_entities;
} EntitySystem;

Either<Error, EntitySystem*> create_entity_system(size_t capacity = ENTITY_SYSTEM_DEFAULT_SIZE);
Error dispose_entity_system(EntitySystem* system);

Error add_entity(EntitySystem* system, const Entity& entity);
Error remove_entity(EntitySystem* system, uint32_t id);

// TODO/later: allow interactions across multiple entity systems and level data
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