
#include "entity.h"
#include "error.h"

#include <algorithm>

static bool check_hitbox_groups(
	Entity* entA, Entity* entB,
	const HitboxGroup* a,
	const HitboxGroup* b) 
{
	int ax = entA->position.x;
	int ay = entA->position.y;
	int bx = entB->position.x;
	int by = entB->position.y;

	for (int i = 0; i < a->n_hitboxes; ++i) {
		const Hitbox* hitboxA = &a->hitboxes[i];
		for (int j = 0; j < b->n_hitboxes; ++j) {
			const Hitbox* hitboxB = &b->hitboxes[j];

			if (hitboxes_overlap(hitboxA, ax, ay, hitboxB, bx, by)) {
				return true;
			}
		}
	}
	return false;
}

static void detect_collisions(Entity* a, Entity* b, EventBuffer* eventBuffer) {
	// TODO: sweep and prune on distance squared

	// Do not collide entities that don't self-collide
	if (a->e_class == b->e_class && !a->e_class->self_colliding) return;

	for (int i = 0; i < a->curFrame->n_hitboxes; ++i) {
		const HitboxGroup* collisionA = &a->curFrame->hitbox_groups[i];
		for (int j = 0; j < b->curFrame->n_hitboxes; ++j) {
			const HitboxGroup* collisionB = &b->curFrame->hitbox_groups[j];

			// If no flags match, do not check collision
			if (collisionA->flags & collisionB->flags == 0) continue;
			if (!hitbox_types_collide(collisionA->type, collisionB->type)) continue;

			bool wasCollision;
			if (hitbox_acts_on(collisionA->type, collisionB->type)) {
				wasCollision = check_hitbox_groups(a, b, collisionA, collisionB);
			}
			else {  // reversed
				wasCollision = check_hitbox_groups(b, a, collisionB, collisionA);
			}
			if (wasCollision) {
				// TODO: priority (where applicable)
				if (eventBuffer != nullptr) {
					Event ev;
					ev.type = Event::Collision;
					ev.collision = {
						collisionA, collisionB, a, b
					};
					push_event(eventBuffer, ev);
				}
			}
		}
	}
}

// pixel distance to consider "close enough" to a contact point
#define CONTACT_EPSILON 0.1f
// maximum speed in pixels per update to eject entities that are somehow colliding without moving.
#define EJECT_VELOCITY 100.f

static void move_to_contact_position(
	Entity* a, Entity* b,
	const HitboxGroup* hitA,
	const HitboxGroup* hitB) {

	Point2 a_init = a->last_pos;
	Point2 b_init = b->last_pos;
	Vector2 a_move = a->position - a->last_pos;
	Vector2 b_move = b->position - b->last_pos;
	float a_mag = a_move.magnitude();
	float b_mag = b_move.magnitude();
	float mag = a_mag + b_mag;
	if (mag <= 0) {
		// Fall back on velocity
		a_move = a->velocity;
		b_move = b->velocity;
		a_mag = a_move.magnitude();
		b_mag = b_move.magnitude();
		mag = a_mag + b_mag;

		if (mag <= 0) {
			// Weird. They don't seem to be moving, so I'm not sure where they came from.
			// But I still have to eject these entities somehow...
			// I guess we'll eject entity A up and entity B down...
			a_init.y -= EJECT_VELOCITY;
			b_init.y += EJECT_VELOCITY;
			a_move = { 0, +EJECT_VELOCITY };
			b_move = { 0, -EJECT_VELOCITY };
			a_mag = EJECT_VELOCITY;
			b_mag = EJECT_VELOCITY;
			mag = 2 * EJECT_VELOCITY;
			// We've done all we can do. If it's even possible for this to occur naturally (it probably is),
			// I commend whoever exploits it in a speedrun or something of that nature.
		}
	}
	// All this to avoid division by zero...

	float rel_dis_a = a_mag / (a_mag + b_mag);
	float rel_dis_b = 1.f - rel_dis_a;

	// OPTIMIZE?/later use something faster than a binary search
	float search_pos = 0.f;
	float search_diff = 0.5f;
	while (search_diff * mag > CONTACT_EPSILON) {
		a->position = a_init + (search_pos * rel_dis_a * a_move);
		b->position = b_init + (search_pos * rel_dis_b * b_move);
		if (check_hitbox_groups(a, b, hitA, hitB)) {
			// back off
			search_pos -= search_diff;
		}
		else {
			// go a little farther
			search_pos += search_diff;
		}
		search_diff /= 2;

		if (search_pos < 0.f) {
			a->position = a_init;
			b->position = b_init;
			break;
		}
	}

	// TODO: if this is a vertical collision (how to detect?), set the ground entity for the higher entity.

	// CONSIDER: 3 solid entities collide at the same time... How should that be resolved?
}

Either<Error, EntitySystem*> create_entity_system(size_t capacity) {
	if (capacity <= 0) {
		return Errors::EntitySystemInvalidSize;
	}

	auto eBuffer = create_EventBuffer();
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
	delete[] system->z_ordered_entities;
	delete system;
	return SUCCESS;
}

Either<Error, Entity*> spawn_entity(EntitySystem* system, const EntityClass* entity_class, Point2 position) {
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
// Run update scripts, which are allowed to spawn entities
// Move entities and advance animations in parallel
// Collision detection in parallel -> generating events in shared buffer
// Process events in main thread (cross-entity interactions are not threadsafe)
void process_entities(const int delta_time, EntitySystem* system) {
	const float dt = ((float) delta_time) / 1000.0f;

	// TODO/later run scripts

	// Update step: position & frame
	size_t processed = 0;
	for (int i = 0; i < system->max_entities; ++i) {
		if (!system->has_entity[i]) continue;

		Entity* e = &system->entities[i];
		e->last_pos = e->position;
		e->last_dt = dt;

		// Apply velocity and acceleration
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

		// Now that we know no new entities will be spawned, get the entities ready to sort.
		system->z_ordered_entities[processed] = e;

		if (++processed >= system->n_entities) break;
	}

	// Sort the entities for display purposes
	std::stable_sort(system->z_ordered_entities, system->z_ordered_entities + system->n_entities,
		[](Entity* a, Entity* b) -> bool {return a->z_order < b->z_order;}
	);

	// COLLISION DETECTION O_O
	// TODO: parallelize!
	for (int i = 0; i < system->n_entities; ++i) {
		Entity* a = system->z_ordered_entities[i];

		for (int j = i + 1; j < system->n_entities; ++j) {
			Entity* b = system->z_ordered_entities[j];

			detect_collisions(a, b, system->event_buffer);
		}
	}

	while (has_events(system->event_buffer)) {
		auto poss_ev = pop_event(system->event_buffer);
		if (poss_ev.isLeft) {
			printf("Event Queue error %d: %s\n", poss_ev.left.code, poss_ev.left.description);
			continue;
		}
		else {
			Event& ev = poss_ev.right;
			Entity* a = ev.collision.entityA;
			Entity* b = ev.collision.entityB;
			const HitboxGroup *hitA = ev.collision.hitboxA;
			const HitboxGroup *hitB = ev.collision.hitboxB;

			printf("COLLISION between entities %d and %d (%s -> %s)\n",
				a, b,
				name_of(hitA->type),
				name_of(hitB->type)
			);

			if (hitA->type == HitboxType::SOLID &&
				hitB->type == HitboxType::SOLID) {
				float a_dx = a->position.x - a->last_pos.x;
				float a_dy = a->position.y - a->last_pos.y;
				float b_dx = b->position.x - b->last_pos.x;
				float b_dy = b->position.y - b->last_pos.y;
				move_to_contact_position(a, b, hitA, hitB);
			}
		}
	}
}

void render_entities(SDL_Renderer* context, const EntitySystem* system) {
	for (int i = 0; i < system->n_entities; ++i) {
		Entity* e = system->z_ordered_entities[i];

		render_hitboxes(context, { (int) e->position.x, (int) e->position.y }, e->curFrame);
	}
}