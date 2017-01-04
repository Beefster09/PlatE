#pragma once

#include <cstdint>
#include <cmath>
#include <cassert>
#include <vector>

#include "sprite.h"
#include "hitbox.h"
#include "either.h"
#include "event.h"
#include "vectors.h"
#include "storage.h"
#include "transform.h"
#include "executor.h"

#include "angelscript.h"

#include "SDL_gpu.h"

#define ENTITY_SYSTEM_DEFAULT_SIZE 256

namespace Errors {
	const error_data
		EntityNotFound = { 404, "Entity does not exist in this system!" },
		EntityMissingInit          = { 301, "Entity behavior component does not have an appropriate init function." },
		EntityInitException        = { 302, "Entity behavior component init() threw an exception." },
		EntityInitUnknownFailure   = { 309, "Entity behavior component init() failed in an unexpected way." },
		EntityUpdateException      = { 312, "Entity behavior component update() threw an exception." },
		EntityUpdateUnknownFailure = { 319, "Entity behavior component init() failed in an unexpected way." };
}

typedef uint32_t EntityId;

const int EntityInvisibleZ = INT_MIN;

// Instances of entities
struct Entity {
// === Metadata ===
	const EntityId id;

// === Physics Data ===
	Point2 position       = { 0.f,0.f };

	Vector2 velocity      = { 0.f,0.f };
	Vector2 acceleration  = { 0.f,0.f };

	// Needed for tunnelling prevention and effects such as motion blur.
	Point2 last_pos       = { 0.f,0.f };

	// Other movement stuffs
	Vector2 max_speed = { INFINITY, INFINITY }; // Maximum speed on each axis
	Vector2 gravity   = { 0.f,0.f };   // Additional acceleration to apply when in midair

	struct GroundLink {
		enum : char {
			NONE = 0,
			MIDAIR = 'm',
			ENTITY = 'e',
			LEVEL = 'l'
		} type = NONE;
		union {
			Entity* entity;
			void* level;
		};
		// Where this entity's foot position is relative to the linked origin.
		Vector2 foot_pos;
		Vector2 contact_normal;
	} ground;

// === Transform Data ===
	float rotation = 0.f;
	Vector2 scale = { 1.f, 1.f };

	// Cached transformation matrix. Calculated during physics/movement step and after update.
	Transform tx = Transform::identity;

// === Render Data ===
	const Sprite* sprite = nullptr;
	const Animation* animation = nullptr;
	const Frame* frame = nullptr;
	uint32_t anim_frame = 0;
	float frame_time = 0.f;

	int z_order = EntityInvisibleZ;

// === Enable/Disable flags ===
	bool physics_enabled = true;
	bool collision_enabled = true;
	bool animation_enabled = true;
	bool rendering_enabled = true;

// === Script Interface ===
	asIScriptObject* rootcomp = nullptr;
	asITypeInfo* rootclass = nullptr;
	asIScriptFunction* updatefunc = nullptr;

// === Functionality ===
	Entity(EntityId id, asIScriptObject* behavior);
	~Entity();

	// In order to keep errors as return values (not throwing exceptions), init must be separate;
	Result<> init(asIScriptEngine* engine);
	Result<> update(asIScriptEngine* engine, float delta_time);

	void render(GPU_Target* screen) const;
};

class EntitySystem {
	typedef std::vector<Entity*> EntityList;
	typedef EntityList::iterator EIter;
private:
	BucketAllocator<Entity> allocator;
	EntityList entities;
	EntityId next_id;

	EventBuffer* event_buffer;

public:
	Executor executor;

	EntitySystem();
	EntitySystem(const EntitySystem&) = delete;
	EntitySystem(EntitySystem&&) = default;

	~EntitySystem();

	Result<Entity*> spawn(asIScriptObject* rootcomp);
	Result<> destroy(EntityId id);
	Result<> destroy(Entity* ent);

	void update(asIScriptEngine* engine, const float delta_time);

	/// Iterator set for entities - allows for interleaved rendering
	std::pair<EIter, EIter> render_iter();
};

void RegisterEntityTypes(asIScriptEngine* engine);

// TODO: SOA/SIMD particle system
