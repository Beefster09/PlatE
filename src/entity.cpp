
#include "entity.h"
#include "error.h"
#include "transform.h"

#include <algorithm>

void update_tx(Entity* a) {
	a->tx = Transform::scal_rot_trans(a->scale, a->rotation, a->position);
}

static void detect_collisions(Entity* a, Entity* b, EventBuffer* eventBuffer) {
	// TODO: sweep and prune on distance squared

	// Do not collide entities that don't self-collide
	if (a->e_class == b->e_class && !a->e_class->self_colliding) return;
	// Ground-linked entities do not collide
	if (a->ground.type == Entity::GroundLink::ENTITY && a->ground.entity == b) return;
	if (b->ground.type == Entity::GroundLink::ENTITY && b->ground.entity == a) return;

	for (const Collider& collisionA : a->curFrame->collision) {
		for (const Collider& collisionB : b->curFrame->collision) {

			bool wasCollision = false;
			if (CollisionType::acts_on(collisionA.type, collisionB.type) ||
				CollisionType::acts_on(collisionB.type, collisionA.type)) {
				wasCollision = hitboxes_overlap(
					&collisionA.hitbox, a->tx, a->position - a->last_pos,
					&collisionB.hitbox, b->tx, b->position - b->last_pos
				);
			}

			if (wasCollision) {
				// TODO: priority (where applicable)
				if (eventBuffer != nullptr) {
					Event ev;
					ev.type = Event::Collision;
					if (CollisionType::acts_on(collisionA.type, collisionB.type)) {
						ev.collision = {
							&collisionA, &collisionB, a, b
						};
					}
					else {
						ev.collision = {
							&collisionB, &collisionA, b, a
						};
					}
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
	const Collider* hitA,
	const Collider* hitB) {

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
		if (hitboxes_overlap(
			&hitA->hitbox, a->tx, a->position - a_init,
			&hitB->hitbox, b->tx, b->position - b_init
		)) {
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
	update_tx(a);
	update_tx(b);

	//a->tx.print_matrix();
	//(~a->tx).print_matrix();
	//b->tx.print_matrix();
	//(~b->tx).print_matrix();
	//(~b->tx * a->tx).print_matrix();
	//(~a->tx * b->tx).print_matrix();

	// TODO: contact normals
	// TODO: if this is a vertical collision (how to detect?), set the ground entity for the higher entity.
	Vector2 diff = a->position - b->position;
	if (diff.y < 0) {
		// a is above b
		//TODO: more checks

		a->ground.type = Entity::GroundLink::ENTITY;
		a->ground.entity = b;
		a->ground.foot_pos = ~b->tx * (a->tx * a->curFrame->foot);
		a->velocity.y = 0.f;
	}
	else {
		// b is above a
		// TODO: more checks

		b->ground.type = Entity::GroundLink::ENTITY;
		b->ground.entity = a;
		b->ground.foot_pos = ~a->tx * (b->tx * b->curFrame->foot);
		b->velocity.y = 0.f;
	}

	// CONSIDER: 3 solid entities collide at the same time... How should that be resolved?
}

Either<Error, EntitySystem*> create_entity_system(size_t capacity) {
	if (capacity <= 0) {
		return Errors::EntitySystemInvalidSize;
	}

	auto eBuffer = create_EventBuffer();
	if (eBuffer.isLeft) return eBuffer.left;

	return new EntitySystem {
		SparseBucket<Entity>(capacity),
		new Entity*[capacity],
		eBuffer.right,
		1
	};
}

Error dispose_entity_system(EntitySystem* system) {
	// Will probably get more complex later...
	delete[] system->z_ordered_entities;
	delete system;
	// Entity bucket gets cleaned up because RAII
	return SUCCESS;
}

Either<Error, Entity*> spawn_entity(EntitySystem* system, const EntityClass* entity_class, Point2 position) {
	const Sprite* initial_sprite = entity_class->initial_sprite;
	const Animation* initial_animation = &initial_sprite->animations[entity_class->initial_animation];
	return system->entities.add(Entity{
		system->next_id++,
		entity_class,
		position,
		{ 0.0f, 0.0f },
		{ 0.0f, 0.0f },
		position, 0,
		{ INFINITY, INFINITY },
		{ 0.0f, 0.0f },
		{ Entity::GroundLink::MIDAIR },
		0,
		0.0f,
		{ 1.f, 1.f },
		Transform::identity,
		initial_sprite,
		initial_animation,
		0, 0.0f,
		initial_animation->frames[0].frame
	});
}

Option<Error> destroy_entity(EntitySystem* system, Entity* ent) {
	return system->entities.remove(ent);
}

Option<Error> destroy_entity(EntitySystem* system, EntityId id) {
	for (Entity& e : system->entities) {
		if (e.id == id) {
			return system->entities.remove(&e);
		}
	}
	return Errors::NoSuchEntity;
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
	for (Entity& e : system->entities) {
		e.last_pos = e.position;
		e.last_dt = dt;

		// Apply velocity and acceleration
		Vector2 actualAccel = e.acceleration +
			(e.ground.type == Entity::GroundLink::MIDAIR ? e.gravity : Vector2{0.f, 0.f});

		if (fabs(e.velocity.x + actualAccel.x) > e.max_speed.x) {
			actualAccel.x = copysign(e.max_speed.x, e.velocity.x) - e.velocity.x;
		}
		if (fabs(e.velocity.y + actualAccel.y) > e.max_speed.y) {
			actualAccel.y = copysign(e.max_speed.y, e.velocity.y) - e.velocity.y;
		}

		e.position += dt * (dt * actualAccel * 0.5f + e.velocity);
		e.velocity += dt * actualAccel;

		// TODO animations

		// Now that we know no new entities will be spawned, get the entities ready to sort.
		system->z_ordered_entities[processed++] = &e;

		e.tx = Transform::scal_rot_trans(e.scale, e.rotation, e.position);
	}

	// Sort the entities for display purposes
	int n_entities = system->entities.size();
	std::stable_sort(system->z_ordered_entities, system->z_ordered_entities + n_entities,
		[](Entity* a, Entity* b) -> bool {return a->z_order < b->z_order;}
	);

	// Move grounded entities relative to their grounds

	for (int i = 0; i < n_entities; ++i) {
		Entity* e = system->z_ordered_entities[i];

		if (e->ground.type == Entity::GroundLink::ENTITY) {
			// TODO: following slopes and lines

			// Set the position by its foot coordinate
			float dx = e->position.x - e->last_pos.x;
			e->ground.foot_pos.x += dx;
			e->position = e->ground.entity->tx * (e->ground.foot_pos - e->curFrame->foot);

			//printf("Entity %d is now following entity %d as ground...\n", e->id, e->ground.entity->id);
			//printf("    It moved %f pixels in the x direction and is being adjusted...\n", dx);
			//printf("    Adjusting new position to (%f, %f)\n", e->position.x, e->position.y);

			// TODO: check if you've gone off the edge... somehow.
		}
	}

	// COLLISION DETECTION O_O
	// TODO: parallelize!
	for (int i = 0; i < n_entities; ++i) {
		Entity* a = system->z_ordered_entities[i];

		for (int j = i + 1; j < n_entities; ++j) {
			Entity* b = system->z_ordered_entities[j];

			detect_collisions(a, b, system->event_buffer);
		}
	}

	// Process events in the order they arrived
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
			const Collider *hitA = ev.collision.hitboxA;
			const Collider *hitB = ev.collision.hitboxB;

			//printf("COLLISION between entities %d and %d (%s -> %s)\n",
			//	a->id, b,
			//	name_of(hitA->type),
			//	name_of(hitB->type)
			//);

			if (hitA->solid && hitB->solid) {
				float a_dx = a->position.x - a->last_pos.x;
				float a_dy = a->position.y - a->last_pos.y;
				float b_dx = b->position.x - b->last_pos.x;
				float b_dy = b->position.y - b->last_pos.y;
				move_to_contact_position(a, b, hitA, hitB);
			}

			a->tx = Transform::scal_rot_trans(a->scale, a->rotation, a->position);
			b->tx = Transform::scal_rot_trans(b->scale, b->rotation, b->position);
		}
	}
}

void render_entities(SDL_Renderer* context, const EntitySystem* system) {
	int n_entities = system->entities.size();
	for (int i = 0; i < n_entities; ++i) {
		Entity* e = system->z_ordered_entities[i];

		render_colliders(context, e->tx, e->curFrame->collision);
	}
}