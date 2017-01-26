
#include "entity.h"
#include "error.h"
#include "transform.h"
#include "util.h"
#include "level.h"

#include <algorithm>
#include <cmath>

static void detect_collisions(Entity* a, Entity* b, Executor& executor);
static void move_to_contact_position(Entity* a, Entity* b);
static void entity_level_collision(Entity* e, LevelInstance* level);

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
	Result<> ret = Result<>::success;
	if (r == asEXECUTION_FINISHED) {
	}
	else if (r == asEXECUTION_EXCEPTION) {
		ret = Error(Errors::EntityInitException, GetExceptionDetails(ctx));
	}
	else {
		ret = Errors::EntityInitUnknownFailure;
	}
	ctx->Unprepare();
	engine->ReturnContext(ctx);
	return ret;
}

Result<> Entity::update(asIScriptEngine* engine, float delta_time) {
	if (updatefunc != nullptr) {
		asIScriptContext* ctx = engine->RequestContext();

		ctx->Prepare(updatefunc);
		ctx->SetObject(rootcomp);
		ctx->SetArgObject(0, this);
		ctx->SetArgFloat(1, delta_time);

		int r = ctx->Execute();
		Result<> ret = Result<>::success;
		if (r == asEXECUTION_FINISHED) {
		}
		else if (r == asEXECUTION_EXCEPTION) {
			ret = Error(Errors::EntityUpdateException, GetExceptionDetails(ctx));
		}
		else {
			ret = Errors::EntityUpdateUnknownFailure;
		}
		ctx->Unprepare();
		engine->ReturnContext(ctx);
		return ret;
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
	
	Transform tx = get_transform();
	render_hitbox(screen,
		animation->solidity.fixed? Transform::scal_trans(scale, position): tx,
		animation->solidity.hitbox, {127, 127, 127});
	render_colliders(screen, tx, frame->colliders);
}

EntitySystem::EntitySystem() : allocator(), entities(), executor(std::thread::hardware_concurrency()) {
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
	entity->system = this;

	auto res = entity->init(engine);

	if (res) {
		entities.push_back(entity);

		return entity;
	}
	else {
		allocator.free(entity);

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
void EntitySystem::update(asIScriptEngine* engine, LevelInstance* level, const float dt) {

	// 'Dumb' update step- each entity behaves as if it's the only thing in existence [Parallelizable]
	for (Entity* e : entities) {
		executor.exec([e, engine, dt, level]() {
			e->last_pos = e->position;

			// Animations
			if (e->animation_enabled && e->animation->frames[e->anim_frame].delay > 0.f) {
				const auto& frames = e->animation->frames;
				e->frame_time += dt;
				bool changed = false;
				while (e->frame_time > frames[e->anim_frame].delay) {
					e->frame_time -= frames[e->anim_frame].delay;
					e->anim_frame = (e->anim_frame + 1) % frames.size();
					changed = true;
				}
				if (changed) e->frame = frames[e->anim_frame].frame;
			}

			// Update via the script component
			e->update(engine, dt);

			// Apply velocity and acceleration
			if (e->physics_enabled) {
				const Vector2 expectedVel = e->velocity + dt * e->acceleration;

				e->velocity = expectedVel.clamped(e->vel_range);

				// This makes acceleration work smoothly at all framerates
				Vector2 error = e->acceleration * dt;
				const Vector2 diff = expectedVel - e->velocity;
				if (diff.x != 0.f) {
					error.x += diff.x * diff.x / e->acceleration.x;
				}
				if (diff.y != 0.f) {
					error.y += diff.y * diff.y / e->acceleration.y;
				}
				error *= 0.5f;

				e->position += dt * e->velocity - error;
			}

			// Level overlap
			entity_level_collision(e, level);
		});
	}

	executor.wait();

	executor.run_deferred();

	// COLLISION DETECTION O_O
	EIter end = entities.end();
	for (EIter aiter = entities.begin(); aiter != end; ++aiter) {
		Entity* a = *aiter;

		for (EIter biter = aiter + 1; biter != end; ++biter) {
			Entity* b = *biter;

			executor.exec([a, b, this]() {
				detect_collisions(a, b, this->executor);
			});
		}
	}

	executor.run_deferred();
}

std::pair<EntitySystem::EIter, EntitySystem::EIter> EntitySystem::render_iter() {
	// Sort the entities for display purposes
	if (!ordered) {
		std::sort(entities.begin(), entities.end(),
			[](Entity* a, Entity* b) -> bool {
				if (a->rendering_enabled == b->rendering_enabled) {
					return a->z_order < b->z_order;
				}
				else {
					// invisibles are always before visibles
					return !a->rendering_enabled && b->rendering_enabled;
				}
			}
		);
		ordered = true;
	}

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

static void move_to_contact_position(Entity* a, Entity* b) {
	// Displacement
	Vector2 aDis = a->position - a->last_pos;
	Vector2 bDis = b->position - b->last_pos;

	Hitbox hitA = a->animation->solidity.hitbox;
	Hitbox hitB = b->animation->solidity.hitbox;

	Transform aTx = a->get_transform();
	Transform bTx = b->get_transform();

	// Displacement with other entity as frame of reference
	Vector2 relDis = aDis - bDis;

	if (relDis.x == 0.f && relDis.y == 0.f) {
		relDis.y = 1.f; // pretend A is moving down
	}

	// Simplest case is easy to compute
	if (hitA.type == Hitbox::BOX && hitB.type == Hitbox::BOX &&
		float_eq(a->rotation, 0.f) && float_eq(b->rotation, 0.f)) {
		AABB boxA = aTx * hitA.box;
		AABB boxB = bTx * hitB.box;

		Vector2 overlap;
		if (relDis.x > 0.f) {
			overlap.x = boxA.right - boxB.left;
		}
		else if (relDis.x < -0.f) {
			overlap.x = boxA.left - boxB.right;
		}
		else {
			overlap.x = 0.f;
		}

		if (relDis.y > 0.f) {
			overlap.y = boxA.bottom - boxB.top;
		}
		else if (relDis.y < -0.f) {
			overlap.y = boxA.top - boxB.bottom;
		}
		else {
			overlap.y = 0.f;
		}

		if (overlap.x == 0.f && overlap.y == 0.f) return;

		// This is a bit magical. Draw a picture.
		if (relDis.cross(overlap) * overlap.x * overlap.y > 0) {
			// x error
			a->position.x -= overlap.x * (aDis.x / relDis.x);
			b->position.x += overlap.x * (bDis.x / relDis.x);
		}
		else {
			// y error
			a->position.y -= overlap.y * (aDis.y / relDis.y);
			b->position.y += overlap.y * (bDis.y / relDis.y);
		}
	}
	else if (hitA.type == Hitbox::CIRCLE && hitB.type == Hitbox::CIRCLE &&
		float_eq(a->scale.x, a->scale.y) && float_eq(b->scale.x, b->scale.y)) {
		
	}
	else { // The general case is... insane
		// TODO
	}
}

static void detect_collisions(Entity* a, Entity* b, Executor& executor) {
	Transform aTx = a->get_transform();
	Vector2 aDis = a->position - a->last_pos;
	Transform bTx = b->get_transform();
	Vector2 bDis = b->position - b->last_pos;

	if (a->solid && b->solid && hitboxes_overlap(
		a->animation->solidity.hitbox, aTx, aDis,
		b->animation->solidity.hitbox, bTx, bDis
	)) {
		executor.defer([a, b]() {
			move_to_contact_position(a, b);
		});
	}

	for (const Collider& collA : a->frame->colliders) {
		for (const Collider& collB : b->frame->colliders) {

			bool fwd = ColliderType::acts_on(collA.type, collB.type);
			bool bkwd = ColliderType::acts_on(collB.type, collA.type);

			if (fwd || bkwd) {
				if (hitboxes_overlap(
					collA.hitbox, aTx, aDis,
					collB.hitbox, bTx, bDis
				)) {
					// TODO: report events to interested parties
					//if (ColliderGroup::acts_on(collA.type, collB.type)) {
					//}
					//else {
					//}
				}
			}
		}
	}
}

static void entity_level_collision(Entity* e, LevelInstance* level) {
	if (level == nullptr || e == nullptr) return;
	if (!e->solid || e->animation->solidity.hitbox.type == Hitbox::NONE) return; // Ignore disabled/empty hitboxes

	// Then check each tilemap for collision
	for (const auto& tilemap : level->layers) {
		if (tilemap.solid && entity_tilemap_collision(e, tilemap)) {
			// TODO: depenetrate

			// IDEA:
			// depenetrate from nearest tiles to original position first
			// ignore nonsolid tiles
			// for full and partial tiles, treat them as aabb hitboxes and depenetrate normally
			// for topleft=false sloped tiles check only foot position when depenetrating upward
			//   (which should be moved to the animation and be auto-calculated)
			// for topleft=true sloped tiles do the vertical reverse
			// otherwise use a more generalized algorithm
			break;
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
		ctx->SetException(std::to_string(maybesprite.err).c_str());
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

static void SpawnDeferred(EntitySystem* system, asIScriptObject* component, asIScriptFunction* callback) {
	if (callback != nullptr) callback->AddRef();

	system->executor.defer([system, component, callback]() {
		auto res = system->spawn(component);
		if (callback != nullptr) {
			if (!res) {
				DispatchErrorCallback(callback, res.err);
			}
			callback->Release();
		}
	});
}

static void DestroyDeferred(Entity* ent, asIScriptFunction* callback) {
	if (callback != nullptr) callback->AddRef();

	ent->system->executor.defer([ent, callback]() {
		auto res = ent->system->destroy(ent);
		if (callback != nullptr) {
			if (!res) {
				DispatchErrorCallback(callback, res.err);
			}
			callback->Release();
		}
	});
}

static int GetSpriteZOrder(Entity* ent) {
	return ent->z_order;
}

static void SetSpriteZOrder(Entity* ent, int z_order) {
	ent->z_order = z_order;
	ent->system->ordered = false;
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
	r = engine->RegisterObjectProperty("Entity", "AABB vel_range", asOFFSET(Entity, vel_range)); assert(r >= 0);

	// rendering
	r = engine->RegisterObjectProperty("Entity", "float rotation", asOFFSET(Entity, rotation)); assert(r >= 0);
	r = engine->RegisterObjectProperty("Entity", "Vector2 scale", asOFFSET(Entity, scale)); assert(r >= 0);

	r = engine->RegisterObjectMethod("Entity", "int get_z_order()",
		asFUNCTION(GetSpriteZOrder), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = engine->RegisterObjectMethod("Entity", "void set_z_order(int)",
		asFUNCTION(SetSpriteZOrder), asCALL_CDECL_OBJFIRST); assert(r >= 0);

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

	// destroy
	r = engine->RegisterObjectMethod("Entity", "void destroy(ErrorCallback@ err = null)",
		asFUNCTION(DestroyDeferred), asCALL_CDECL_OBJFIRST); assert(r >= 0);

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

	r = engine->RegisterObjectMethod("__EntitySystem__", "void spawn(EntityComponent@, ErrorCallback@ err = null)",
		asFUNCTION(SpawnDeferred), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	
}