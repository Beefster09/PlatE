
#include "entity.h"
#include "error.h"

Either<Error, EntitySystem*> create_entity_system(size_t capacity) {
	// This will probably get more complex later
	return new EntitySystem {
		new Entity[capacity],
		1,
		0,
		capacity
	};
}

Error dispose_entity_system(EntitySystem* system) {
	// Will probably get more complex later...
	delete system->entities;
	delete system;
	return SUCCESS;
}

Error add_entity(EntitySystem* system, const Entity& entity) {
	if (system->n_entities >= system->max_entities) {
		return Errors::EntitySystemCapacityReached;
	}

	int index = system->n_entities++;
	system->entities[index] = entity;
	system->entities[index].index = index;
	system->entities[index].id = system->nextId++;

	return SUCCESS;
}

void process_entities(const int delta_time, EntitySystem* system) {
	const float dt = ((float) delta_time) / 1000.0f;
	// Apply velocity and acceleration
	for (int i = 0; i < system->n_entities; ++i) {
		Entity* e = &system->entities[i];
		e->position.x += dt * (e->acceleration.x / 2.0f + e->velocity.x);
		e->velocity.x += dt * e->acceleration.x;
		e->position.y += dt * (e->acceleration.y / 2.0f + e->velocity.y);
		e->velocity.y += dt * e->acceleration.y;
	}
}

void render_entities(SDL_Renderer* context, const EntitySystem* system) {
	for (int i = 0; i < system->n_entities; ++i) {
		Entity* e = &system->entities[i];
		render_hitboxes(context, { e->position.x, e->position.y }, e->curFrame);
	}
}