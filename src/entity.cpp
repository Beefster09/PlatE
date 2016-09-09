
#include "entity.h"
#include "error.h"

static void check_hitbox_groups(
	Entity* entA, Entity* entB,
	const HitboxGroup* a, int ax, int ay,
	const HitboxGroup* b, int bx, int by,
	EventBuffer* eventBuffer) 
{
	for (int i = 0; i < a->n_hitboxes; ++i) {
		const Hitbox* hitboxA = &a->hitboxes[i];
		for (int j = 0; j < b->n_hitboxes; ++j) {
			const Hitbox* hitboxB = &b->hitboxes[j];

			if (hitboxes_overlap(hitboxA, ax, ay, hitboxB, bx, by)) {
				Event ev;
				ev.type = EventType::Collision;
				ev.collision = {
					a->type, b->type,
					entA, entB
				};
				push_event(eventBuffer, ev);
			}
			return; // Only test each hitbox group once!
		}
	}
}

static void detect_collisions(Entity* a, Entity* b, EventBuffer* eventBuffer) {
	int ax = a->position.x;
	int ay = a->position.y;
	int bx = b->position.x;
	int by = b->position.y;

	// TODO: sweep and prune on AABBs or distance squared

	for (int i = 0; i < a->curFrame->n_hitboxes; ++i) {
		const HitboxGroup* collisionA = &a->curFrame->hitbox_groups[i];
		for (int j = 0; j < b->curFrame->n_hitboxes; ++j) {
			const HitboxGroup* collisionB = &b->curFrame->hitbox_groups[j];

			// If no flags match, do not check collision
			if (collisionA->flags & collisionB->flags == 0) continue;
			if (!hitbox_types_collide(collisionA->type, collisionB->type)) continue;

			if (hitbox_types_order(collisionA->type, collisionB->type)) {
				check_hitbox_groups(a, b, collisionA, ax, ay, collisionB, bx, by, eventBuffer);
			}
			else {  // reversed
				check_hitbox_groups(a, b, collisionB, bx, by, collisionA, ax, ay, eventBuffer);
			}
		}
	}
}

Either<Error, EntitySystem*> create_entity_system(size_t capacity) {
	if (capacity <= 0) {
		return Errors::EntitySystemInvalidSize;
	}

	auto eBuffer = create_EventBuffer(capacity * 4);
	if (eBuffer.isLeft) return eBuffer.left;

	bool* has_entity = new bool[capacity];
	for (int i = 0; i < capacity; ++i) has_entity[i] = false;

	return new EntitySystem {
		new Entity[capacity],
		has_entity,
		new Entity*[capacity],
		eBuffer.right,
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

	printf("SPAWNING!\n");

	int index = system->next_index++;
	const Sprite* initial_sprite = entity_class->initial_sprite;
	const Animation* initial_animation = &initial_sprite->animations[entity_class->initial_animation];
	system->entities[index] = {
		system->next_id++,
		entity_class,
		position,
		{ 0.0f, 0.0f },
		{ 0.0f, 0.0f },
		position, 0,
		{ INFINITY, INFINITY },
		{ 0.0f, 0.0f },
		false,
		0,
		0.0f,
		initial_sprite,
		initial_animation,
		0, 0.0f,
		initial_animation->frames[0].frame
	};
	system->has_entity[index] = true;
	++system->n_entities;

	// TODO/eventually: run init script

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

	// TODO/later run scripts

	// Apply velocity and acceleration
	size_t processed = 0;
	for (int i = 0; i < system->max_entities; ++i) {
		if (!system->has_entity[i]) continue;

		Entity* e = &system->entities[i];
		float actualAccelX = e->acceleration.x + (e->onGround ? 0.f : e->gravity.x);
		float actualAccelY = e->acceleration.y + (e->onGround ? 0.f : e->gravity.y);
		if (fabs(e->velocity.x + actualAccelX) > e->max_speed.x) {
			actualAccelX = copysign(e->max_speed.x, e->velocity.x) - e->velocity.x;
		}
		if (fabs(e->velocity.y + actualAccelY) > e->max_speed.y) {
			actualAccelX = copysign(e->max_speed.y, e->velocity.y) - e->velocity.y;
		}

		e->position.x += dt * (dt * actualAccelX * 0.5f + e->velocity.x);
		e->velocity.x += dt * actualAccelX;
		e->position.y += dt * (dt * actualAccelY * 0.5f + e->velocity.y);
		e->velocity.y += dt * actualAccelY;

		// TODO animations

		if (++processed >= system->n_entities) break;
	}

	// COLLISION DETECTION O_O
	// TODO: parallelize!
	processed = 0;
	for (int i = 0; i < system->max_entities; ++i) {
		if (!system->has_entity[i]) continue;

		Entity* a = &system->entities[i];

		int otherProcessed = 0;
		for (int j = i + 1; j < system->max_entities; ++j) {
			if (!system->has_entity[j]) continue;

			Entity* b = &system->entities[j];

			detect_collisions(a, b, system->event_buffer);
		}

		if (++processed >= system->n_entities) break;
	}

	while (has_events(system->event_buffer)) {
		auto poss_ev = pop_event(system->event_buffer);
		if (poss_ev.isLeft) {
			printf("Event Queue error %d: %s\n", poss_ev.left.code, poss_ev.left.description);
			continue;
		}
		else {
			Event& ev = poss_ev.right;

			printf("COLLISION between entities %d and %d (%s -> %s)\n",
				ev.collision.entityA->id,
				ev.collision.entityB->id,
				name_of(ev.collision.typeA),
				name_of(ev.collision.typeB)
			);
		}
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