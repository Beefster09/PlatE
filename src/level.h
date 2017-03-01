#pragma once

#include "result.h"
#include "tileset.h"
#include "vectors.h"
#include "arrays.h"
#include "hitbox.h"
#include "entity.h"
#include "error.h"
#include "SDL2\SDL_render.h"
#include <cstdint>
#include <array>

#define LEVEL_MAGIC_NUMBER "PlatElevel"
#define LEVEL_MAGIC_NUMBER_LENGTH (sizeof(LEVEL_MAGIC_NUMBER) - 1)

#define LEVEL_BALLPARK_CHECKED true
#define LEVEL_BALLPARK_REQUIRED true

namespace Errors {
	const error_data
		InvalidLevelHeader = { 601, "Level does not begin with the string \"" LEVEL_MAGIC_NUMBER "\"" },
		InvalidLevelHeaderSizes = { 602, "Level header sizes not consistent with the size of the file." };
}

/// Static tile-based level data. Tiles can be animated.
struct Tilemap {
	const Tileset* tileset;
	Array2D<uint16_t> tiles;
	int z_order;

	/// position of this tilemap's top-left corner relative to the level's origin
	Vector2 offset;

	/// how much to scale the entire tilemap (individual tiles and as a whole)
	/// affects collision
	Vector2 scale;

	/// amount this tilemap moves relative to the level.
	// for example:
	// if {0, 0} it will stay stationary always, essentially putting it in viewport space
	// if {1, 1} it will move at the same rate as the level
	// if {0.5, 0.5} it will move half the speed of the viewport
	// etc...
	Vector2 parallax;

	bool solid; // only check for collisions if this is true
};

/// Static elements of the level. Logic cannot be attached.
struct SceneObject {
	const Sprite* sprite;

	Vector2 display;
	int z_order;

	Vector2 position;
	float rotation;
	Vector2 scale;

// === Transient Fields ===
	AABB aabb;
};

/// Points that spawn entities when within the culling range of the viewport
struct EntitySpawnPoint {
	Vector2 location;
};

/// Script triggers along the edge of the level.
/// They trigger when entities intersect part of the boundary of the level
// this allows for things like side warps
struct EdgeTrigger {
	enum Side : char {
		TOP    = 't',
		BOTTOM = 'b',
		LEFT   = 'l',
		RIGHT  = 'r'
	} side;

	/// The center position along that edge
	float position;

	/// Half the width or height (depending on which side) of the trigger
	float size;

	/// The proportion of the triggering entity's edge that must touch the edge to count
	/// Set to 0 if any point of the collision box counts
	/// Set to 1 if all of the entity's corresponding height or width must match
	/// Set to a negative number if the entity needs to completely exit through the side for it to count
	float strictness;

	// Script partial_handler;
	// Script full_handler;
};

struct LevelArea {
	AABB aabb;
	int priority;

	SDL_Color ui_color;

	// Script enter
	// Script exit
	// Script tick
};

#define RTREE_MAX_CHILDREN 4

struct Level {
	const char* name;

	/// the bounds of the level from the origin
	// This helps to determine when to fire edge-entity events
	// Cameras will only view outside the boundary if the level is too small or if a script says so.
	AABB boundary;

	// The actual substance of the level
	Array<const Tilemap> layers;
	Array<const SceneObject> objects;
	Array<const EntitySpawnPoint> entities;

	// Regions of the level with different rules- such as water... ?
	Array<const LevelArea> areas;

	// This modifies what is actually fired for edge-entity interactions
	// Will override a regular edge-entity event with either a partial or full edge trigger event
	// Overlapping triggers will take first-come-first serve priority
	Array<const EdgeTrigger> edge_triggers;

	// Script / save-relevant metadata
	// ---- Something like this:
	// Script* init_script;
	// Array<Variable> variables; 

// === Transient / Precalculated Cached T ===
	struct RTree {
		AABB bounding_rect;

		size_t n_children;
		struct {
			union {
				SceneObject* obj;
				RTree* subtree;
			};
			bool isLeaf;
		} children[RTREE_MAX_CHILDREN];
	};

	RTree* spacial_index;
};

/// Mutable variant of a Level
struct LevelInstance {
	const Level* base;
	Array<Tilemap> layers;
	Array<Array<TileAnimationState>> anim_state;
};

// Note to self: scripts will probably be able to splice to Levels together
// They will definitely be able to trigger side-warps

// static to .cpp: RTree* build_spacial_index(Level*);

Result<const Level*> load_level(const char* filename, const DirContext& context = DirContext());

Result<> unload_level(const Level*);

void render_tilemap(GPU_Target* context, const Tilemap* map); // TODO: camera/viewport

/// Range of tile indices, inclusive on both ends
struct TileRange {
	uint16_t left, right, top, bottom;
};

/// Returns the range of tiles within a certain AABB. Includes tiles that are only partially in the AABB.
TileRange tiles_in(const Tilemap& map, const AABB& region);

LevelInstance* instantiate_level(const Level* level);
inline void destroy_level_instance(LevelInstance* inst) { operator delete(inst); }

bool entity_tilemap_collision(const Entity* e, const Tilemap& map);