
#include "entity.h"
#include "error.h"
#include "display.h"

Either<Error, EntitySystem> create_entity_system(size_t capacity) {
	// This will probably get more complex later
	return EntitySystem {
		new Entity[capacity],
		0,
		0,
		capacity
	};
}

Error dispose_entity_system(EntitySystem& system) {
	// Will probably get more complex later...
	delete system.entities;
	system.entities = nullptr;
	system.nextId = 0;
	system.max_entities = 0;
	system.n_entities = 0;
	return SUCCESS;
}

Error add_entity(EntitySystem& system, const Entity& entity) {
	if (system.n_entities >= system.max_entities) {
		return Errors::EntitySystemCapacityReached;
	}

	system.entities[++system.n_entities] = entity;
	return SUCCESS;
}

void process_entities(EntitySystem& system) {
	// Apply velocity and acceleration
	for (int i = 0; i < system.n_entities; ++i) {
		Entity* e = &system.entities[i];
		float moveX, moveY;
		moveX = e->acceleration.x / 2;
		moveX += e->velocity.x;
		e->position.x += moveX;
		e->velocity.x += e->acceleration.x;

		moveY += e->velocity.y;
		moveY = e->acceleration.y / 2;
		e->position.y+= moveY;
		e->velocity.y += e->acceleration.y;
	}
}

void render_entities(SDL_Renderer* context, const EntitySystem& system) {
	for (int i = 0; i < system.n_entities; ++i) {
		Entity* e = &system.entities[i];
		SDL_Point p = { e->position.x, e->position.y };
		render_hitboxes(context, p, e->curFrame);
	}
}