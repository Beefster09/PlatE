
#include "entity.h"
#include "error.h"
#include "transform.h"
#include "util.h"

#include <algorithm>
#include <cmath>

void update_tx(Entity* a) {
	a->tx = Transform::scal_rot_trans(a->scale, a->rotation, a->position);
}

static void detect_collisions(Entity* a, Entity* b, EventBuffer* eventBuffer);
static void move_to_contact_position(Entity* a, Entity* b, const Collider* hitA, const Collider* hitB);

Entity::Entity(EntityId id, asIScriptObject* behavior) : id(id) {
	assert(behavior != nullptr);
	rootcomp = behavior;

	rootclass = behavior->GetObjectType();
	assert(rootclass != nullptr);

	updatefunc = rootclass->GetMethodByDecl("void update(Entity@, float)");

	rootcomp->AddRef();
}

Entity::~Entity() {
	rootcomp->Release();
}

// In order to keep errors as return values (not throwing exceptions), init must be separate;
Result<> Entity::init(asIScriptEngine* engine) {
	asIScriptFunction* func = rootclass->GetMethodByDecl("void init(Entity@)");
	if (func == nullptr) {
		return Errors::EntityMissingInit;
	}

	asIScriptContext* ctx = engine->RequestContext();

	ctx->Prepare(func);
	ctx->SetObject(rootcomp);
	ctx->SetArgObject(0, this);

	int r = ctx->Execute();
	if (r == asEXECUTION_FINISHED) {
		ctx->Unprepare();
		engine->ReturnContext(ctx);
		return Result<>::success;
	}
	else if (r == asEXECUTION_EXCEPTION) {
		printf("Exception in init(): %s\n", ctx->GetExceptionString());
		ctx->Unprepare();
		engine->ReturnContext(ctx);
		return Errors::EntityInitException;
	}
	else {
		ctx->Unprepare();
		engine->ReturnContext(ctx);
		return Errors::EntityInitUnknownFailure;
	}
}

Result<> Entity::update(asIScriptEngine* engine, float delta_time) {
	if (updatefunc != nullptr) {
		asIScriptContext* ctx = engine->RequestContext();

		ctx->Prepare(updatefunc);
		ctx->SetObject(rootcomp);
		ctx->SetArgObject(0, this);
		ctx->SetArgFloat(1, delta_time);

		int r = ctx->Execute();
		if (r == asEXECUTION_FINISHED) {
			ctx->Unprepare();
			engine->ReturnContext(ctx);
		}
		else if (r == asEXECUTION_EXCEPTION) {
			printf("Exception in init(): %s\n", ctx->GetExceptionString());
			ctx->Unprepare();
			engine->ReturnContext(ctx);
			return Errors::EntityUpdateException;
		}
		else {
			return Errors::EntityUpdateUnknownFailure;
		}
	}

	return Result<>::success;
}

void Entity::render(GPU_Target* screen) const {
	const Frame* frame = animation->frames[anim_frame].frame;
	Vector2 display = frame->display;

	GPU_BlitTransformX(
		sprite->texture, const_cast<GPU_Rect*>(frame->clip), screen,
		position.x, position.y, -display.x, -display.y,
		rad_to_deg(rotation), scale.x, scale.y
	);

	render_colliders(screen, tx, frame->collision);
}

EntitySystem::EntitySystem() : allocator(), entities(), executor(std::thread::hardware_concurrency()) {
	auto res = create_EventBuffer();
	if (res) {
		event_buffer = res.value;
	}

	next_id = 1000;
}

EntitySystem::~EntitySystem() {
	// The allocator should auto-delete entities.

	// This isn't exactly a missions-critical function at the moment,
	//   so I don't suspect this will be much of a problem... yet
}

Result<Entity*> EntitySystem::spawn(asIScriptObject* rootcomp) {
	asIScriptEngine* engine = rootcomp->GetEngine();
	Entity* entity = allocator.alloc();
	if (entity == nullptr) {
		return Errors::BadAlloc;
	}
	new(entity) Entity(next_id++, rootcomp);

	auto res = entity->init(engine);

	if (res) {
		entities.push_back(entity);

		return entity;
	}
	else {
		return res.err;
	}
}

Result<> EntitySystem::destroy(Entity* ent) {
	auto ind = std::find(entities.begin(), entities.end(), ent);
	if (ind != entities.end()) {
		allocator.free(ent);
		entities.erase(ind);
		return Result<>::success;
	}
	else {
		return Errors::EntityNotFound;
	}
}

Result<> EntitySystem::destroy(EntityId id) {
	auto ind = std::find_if(entities.begin(), entities.end(),
		[id](Entity* e) -> bool { return e->id == id; });
	if (ind != entities.end()) {
		allocator.free(*ind);
		entities.erase(ind);
		return nullptr;
	}
	else {
		return Errors::EntityNotFound;
	}
}

// High level algorithm:
// Run update scripts, which are allowed to spawn entities
// Move entities and advance animations in parallel
// Collision detection in parallel -> generating events in shared buffer
// Process events in main thread (cross-entity interactions are not threadsafe)
void EntitySystem::update(asIScriptEngine* engine, const float dt) {

	// 'Dumb' update step- each entity behaves as if it's the only thing in existence [Parallelizable]
	for (Entity* e : entities) {
		e->last_pos = e->position;

		// Animations
		if (e->animation_enabled) {
			// TODO
		}

		// Update via the script component
		e->update(engine, dt);

		// Apply velocity and acceleration
		if (e->physics_enabled) {
			Vector2 actualAccel = e->acceleration +
				(e->ground.type == Entity::GroundLink::MIDAIR ? e->gravity : Vector2{ 0.f, 0.f });

			if (fabs(e->velocity.x + actualAccel.x) > e->max_speed.x) {
				actualAccel.x = copysign(e->max_speed.x, e->velocity.x) - e->velocity.x;
			}
			if (fabs(e->velocity.y + actualAccel.y) > e->max_speed.y) {
				actualAccel.y = copysign(e->max_speed.y, e->velocity.y) - e->velocity.y;
			}

			// the dt * accel * 0.5f is a wee bit of calculus to make motion consistent at all framerates
			e->position += dt * (dt * actualAccel * 0.5f + e->velocity);
			e->velocity += dt * actualAccel;

			e->tx = Transform::scal_rot_trans(e->scale, e->rotation, e->position);
		}
	}

	executor.run_deferred();

	// Move grounded entities relative to their grounds

	for (Entity* e : entities) {
		if (e->ground.type == Entity::GroundLink::ENTITY) {
			// TODO: following slopes and lines

			// Set the position by its foot coordinate
			float dx = e->position.x - e->last_pos.x;
			e->ground.foot_pos.x += dx;
			e->position = e->ground.entity->tx * (e->ground.foot_pos - e->frame->foot);

			//printf("Entity %d is now following entity %d as ground...\n", e->id, e->ground.entity->id);
			//printf("    It moved %f pixels in the x direction and is being adjusted...\n", dx);
			//printf("    Adjusting new position to (%f, %f)\n", e->position.x, e->position.y);

			// TODO: check if you've gone off the edge... somehow.
		}
	}

	// COLLISION DETECTION O_O
	// TODO: parallelize!
	EIter end = entities.end();
	for (EIter aiter = entities.begin(); aiter != end; ++aiter) {
		Entity* a = *aiter;

		for (EIter biter = aiter + 1; biter != end; ++biter) {
			Entity* b = *biter;

			detect_collisions(a, b, event_buffer);
		}
	}

	// Process events in the order they arrived
	while (has_events(event_buffer)) {
		auto poss_ev = pop_event(event_buffer);
		if (!poss_ev) {
			printf("Event Queue error %d: %s\n", poss_ev.err.code, poss_ev.err.description);
			continue;
		}
		else {
			Event& ev = poss_ev.value;
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

std::pair<EntitySystem::EIter, EntitySystem::EIter> EntitySystem::render_iter() {
	// Sort the entities for display purposes
	std::stable_sort(entities.begin(), entities.end(),
		[](Entity* a, Entity* b) -> bool {
			// invisibles are always before visibles
			if (a->rendering_enabled == b->rendering_enabled) {
				return a->z_order < b->z_order;
			}
			else {
				return !a->rendering_enabled && b->rendering_enabled;
			}
		}
	);

	EIter begin = entities.begin();
	EIter end = entities.end();

	// skip over invisibles
	while (begin < end && !(*begin)->rendering_enabled) ++begin;

	return make_pair(begin, end);
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

	// TODO: contact normals
	// TODO: if this is a vertical collision (how to detect?), set the ground entity for the higher entity.
	Vector2 diff = a->position - b->position;
	if (diff.y < 0) {
		// a is above b
		//TODO: more checks

		a->ground.type = Entity::GroundLink::ENTITY;
		a->ground.entity = b;
		a->ground.foot_pos = ~b->tx * (a->tx * a->frame->foot);
		a->velocity.y = 0.f;
	}
	else {
		// b is above a
		// TODO: more checks

		b->ground.type = Entity::GroundLink::ENTITY;
		b->ground.entity = a;
		b->ground.foot_pos = ~a->tx * (b->tx * b->frame->foot);
		b->velocity.y = 0.f;
	}

	// CONSIDER: 3 solid entities collide at the same time... How should that be resolved?
}

static void detect_collisions(Entity* a, Entity* b, EventBuffer* eventBuffer) {
	// TODO: sweep and prune on distance squared

	// Ground-linked entities do not collide
	if (a->ground.type == Entity::GroundLink::ENTITY && a->ground.entity == b) return;
	if (b->ground.type == Entity::GroundLink::ENTITY && b->ground.entity == a) return;

	for (const Collider& collisionA : a->frame->collision) {
		for (const Collider& collisionB : b->frame->collision) {

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

// =========================================================================================
// ==== AngelScript Interface ====
// =========================================================================================

static void SetEntitySprite(Entity* entity, const std::string& filename) {
	auto maybesprite = load_sprite(filename.c_str());
	if (maybesprite) {
		entity->sprite = maybesprite;
		entity->animation = &(entity->sprite->animations[0]);
		entity->frame = entity->animation->frames[0].frame;
		entity->frame_time = 0.f;
		entity->anim_frame = 0;
	}
	else {
		asIScriptContext* ctx = asGetActiveContext();
		ctx->SetException(maybesprite.err.description);
	}
}

static void SetEntityAnimationByIndex(Entity* entity, int index) {
	if (entity->sprite == nullptr) {
		asIScriptContext* ctx = asGetActiveContext();
		ctx->SetException("Sprite has not been initialized");
	}
	else if (index < 0 || index >= entity->sprite->animations.size()) {
		asIScriptContext* ctx = asGetActiveContext();
		ctx->SetException("Index out of bounds");
	}
	else {
		entity->animation = &(entity->sprite->animations[index]);
		entity->frame = entity->animation->frames[0].frame;
		entity->frame_time = 0.f;
		entity->anim_frame = 0;
	}
}

static void SetEntityAnimationByName(Entity* entity, const std::string& name) {
	if (entity->sprite == nullptr) {
		asIScriptContext* ctx = asGetActiveContext();
		ctx->SetException("Sprite has not been initialized");
	}

	const auto& anims = entity->sprite->animations;
	const Animation* anim = std::find_if(anims.begin(), anims.end(),
		[name = name.c_str()](const Animation& anim) -> bool { return strcmp(anim.name, name) == 0; });

	if (anim == anims.end()) { // not found
		asIScriptContext* ctx = asGetActiveContext();
		ctx->SetException("No animation in the sprite by that name");
	}
	else {
		entity->animation = anim;
		entity->frame = anim->frames[0].frame;
		entity->frame_time = 0.f;
		entity->anim_frame = 0;
	}
}

static void SetEntitySpriteCompound(Entity* entity, const std::string& filename, const std::string& animname) {
	auto maybesprite = load_sprite(filename.c_str());
	if (maybesprite) {
		entity->sprite = maybesprite;
		SetEntityAnimationByName(entity, animname);
	}
	else {
		asIScriptContext* ctx = asGetActiveContext();
		ctx->SetException(maybesprite.err.description);
	}
}

static void SetEntitySpriteCompound(Entity* entity, const std::string& filename, int animindex) {
	auto maybesprite = load_sprite(filename.c_str());
	if (maybesprite) {
		entity->sprite = maybesprite;
		SetEntityAnimationByIndex(entity, animindex);
	}
	else {
		asIScriptContext* ctx = asGetActiveContext();
		ctx->SetException(maybesprite.err.description);
	}
}

static void SpawnDeferred(EntitySystem* system, asIScriptObject* component) {
	system->executor.defer([system, component]() {
		auto res = system->spawn(component);
		if (res) {
			printf("Successfully spawned entity\n");
		}
		else {
			printf("Encountered error while spawning entity: %s\n", res.err.description);
		}
	});
}

void RegisterEntityTypes(asIScriptEngine* engine) {
	int r;

	// ==============
	// === Entity ===
	// ==============

	r = engine->RegisterObjectType("Entity", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

	// metadata
	r = engine->RegisterObjectProperty("Entity", "const uint id", asOFFSET(Entity, id)); assert(r >= 0);

	// physics
	r = engine->RegisterObjectProperty("Entity", "Vector2 position", asOFFSET(Entity, position)); assert(r >= 0);
	r = engine->RegisterObjectProperty("Entity", "const Vector2 last_pos", asOFFSET(Entity, last_pos)); assert(r >= 0);

	r = engine->RegisterObjectProperty("Entity", "Vector2 velocity", asOFFSET(Entity, velocity)); assert(r >= 0);
	r = engine->RegisterObjectProperty("Entity", "Vector2 acceleration", asOFFSET(Entity, acceleration)); assert(r >= 0);
	r = engine->RegisterObjectProperty("Entity", "Vector2 gravity", asOFFSET(Entity, gravity)); assert(r >= 0);

	// rendering
	r = engine->RegisterObjectProperty("Entity", "float rotation", asOFFSET(Entity, rotation)); assert(r >= 0);
	r = engine->RegisterObjectProperty("Entity", "Vector2 scale", asOFFSET(Entity, scale)); assert(r >= 0);

	// sprite
	r = engine->RegisterObjectMethod("Entity", "void set_sprite(const string &in)",
		asFUNCTION(SetEntitySprite), asCALL_CDECL_OBJFIRST); assert(r >= 0);

	r = engine->RegisterObjectMethod("Entity", "void set_animation(const string &in)",
		asFUNCTION(SetEntityAnimationByName), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = engine->RegisterObjectMethod("Entity", "void set_animation(int)",
		asFUNCTION(SetEntityAnimationByIndex), asCALL_CDECL_OBJFIRST); assert(r >= 0);

	r = engine->RegisterObjectMethod("Entity", "void set_sprite(const string &in, const string &in)",
		asFUNCTIONPR(SetEntitySpriteCompound, (Entity*, const std::string&, const std::string&), void),
		asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = engine->RegisterObjectMethod("Entity", "void set_sprite(const string &in, int)",
		asFUNCTIONPR(SetEntitySpriteCompound, (Entity*, const std::string&, int), void),
		asCALL_CDECL_OBJFIRST); assert(r >= 0);

	// enable/disable flags
	r = engine->RegisterObjectProperty("Entity", "bool physics_enabled", asOFFSET(Entity, physics_enabled)); assert(r >= 0);
	r = engine->RegisterObjectProperty("Entity", "bool collision_enabled", asOFFSET(Entity, collision_enabled)); assert(r >= 0);
	r = engine->RegisterObjectProperty("Entity", "bool animation_enabled", asOFFSET(Entity, animation_enabled)); assert(r >= 0);
	r = engine->RegisterObjectProperty("Entity", "bool visible", asOFFSET(Entity, rendering_enabled)); assert(r >= 0);

	// Entity Component Interface
	r = engine->RegisterInterface("EntityComponent"); assert(r >= 0);
	r = engine->RegisterInterfaceMethod("EntityComponent", "void init(Entity@)"); assert(r >= 0);
	r = engine->RegisterInterfaceMethod("EntityComponent", "void update(Entity@, float)"); assert(r >= 0);

	// ====================
	// === EntitySystem ===
	// ====================

	r = engine->RegisterObjectType("__EntitySystem__", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

	r = engine->RegisterObjectMethod("__EntitySystem__", "void spawn(EntityComponent@)",
		asFUNCTION(SpawnDeferred), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	
}