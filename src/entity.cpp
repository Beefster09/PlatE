
#include "entity.h"
#include "error.h"

Either<Error, EntitySystem*> create_entity_system(size_t capacity) {
	if (capacity <= 0) {
		return Errors::EntitySystemInvalidSize;
	}

	bool* has_entity = new bool[capacity];
	for (int i = 0; i < capacity; ++i) has_entity[i] = false;

	return new EntitySystem {
		new Entity[capacity],
		has_entity,
		new Entity*[capacity],
		1,
		0,
		0,
		capacity
	};
}

Error dispose_entity_system(EntitySystem* system) {
	// Will probably get more complex later...
	delete[] system->entities;
	delete[] system->has_entity;
	delete system;
	return SUCCESS;
}

Either<Error, Entity*> spawn_entity(EntitySystem* system, const EntityClass* entity_class, PixelPosition position) {
	if (system->n_entities >= system->max_entities) {
		return Errors::EntitySystemCapacityReached;
	}

	int index = system->next_index++;
	const Sprite* initial_sprite = entity_class->initial_sprite;
	const Animation* initial_animation = &initial_sprite->animations[entity_class->initial_animation];
	system->entities[index] = {
		system->next_id++,
		entity_class,
		position,
		{0.0f, 0.0f},
		{0.0f, 0.0f},
		position, 0,
		0,
		0.0f,
		initial_sprite,
		initial_animation,
		0, 0.0f,
		initial_animation->frames[0].frame
	};
	system->has_entity[index] = true;
	++system->n_entities;

	if (system->n_entities < system->max_entities) { // there must be an open slot!
		while (system->has_entity[system->next_index]) { // Search for the next available slot
			system->next_index = (system->next_index + 1) % system->max_entities;
			if (system->next_index == index) {
				return DetailedError(Errors::EntitySystemInvalidState,
					"INVALID ENTITY SYSTEM STATE: There should be an available index, but there isn't one!\n");
			}
		}
	}

	return &system->entities[index];
}

// High level algorithm:
// Move entities and advance animations in parallel
// Collision detection in parallel -> generating events in shared buffer(s?)
// Process events in main thread (cross-entity interactions are not threadsafe)
void process_entities(const int delta_time, EntitySystem* system) {
	const float dt = ((float) delta_time) / 1000.0f;
	// Apply velocity and acceleration
	size_t processed = 0; // Macro?
	for (int i = 0; i < system->max_entities; ++i) {
		if (!system->has_entity[i]) continue;

		Entity* e = &system->entities[i];
		e->position.x += dt * (dt * e->acceleration.x * 0.5f + e->velocity.x);
		e->velocity.x += dt * e->acceleration.x;
		e->position.y += dt * (dt * e->acceleration.y * 0.5f + e->velocity.y);
		e->velocity.y += dt * e->acceleration.y;

		// TODO animations

		if (++processed >= system->n_entities) break;
	}
}

void render_entities(SDL_Renderer* context, const EntitySystem* system) {
	size_t rendered = 0;
	for (int i = 0; i < system->max_entities; ++i) {
		if (!system->has_entity[i]) continue;

		Entity* e = &system->entities[i];
		render_hitboxes(context, { e->position.x, e->position.y }, e->curFrame);

		if (++rendered >= system->n_entities) break;
	}
}