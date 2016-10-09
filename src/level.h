#pragma once

#include "tileset.h"
#include "vectors.h"
#include "storage.h"
#include "hitbox.h"
#include "entity.h"
#include "SDL2\SDL_render.h"
#include <cstdint>
#include <array>

struct Tilemap {
	const Tileset* tileset;
	Array2D<uint32_t> tiles;
	int z_order;
	Vector2 parallax;
};

struct AABB {
	float x, y, w, h;
};

struct SceneObject {
	SDL_Texture* texture;
	SDL_Rect clip;
	Vector2 position;
	float rotation;
	Vector2 scale;
	CollisionData collision;
	AABB aabb;
};

struct EntitySpawnPoint {
	float x, y;
	const char* e_class; // (file)name of entity class

	// Spawn Parameters:
	// ---- Something like:
	// SpawnParams params;
};

#define RTREE_MAX_CHILDREN 4

struct Level {
	const char* name;
	Array<const Tilemap> layers;
	Array<const SceneObject> objects;
	Array<const EntitySpawnPoint> entities;

	struct RTree {
		AABB bounding_rect;

		size_t n_children;
		Either<const SceneObject*, RTree*> children[RTREE_MAX_CHILDREN];
	};

	RTree* spacial_index;

	// Script / save-relevant metadata
	// ---- Something like this:
	// Script* init_script;
	// Array<Variable> variables; 
};

// static to .cpp: RTree* build_spacial_index(Level*);

Either<Error, Level*> load_level(const char* filename);

Either<Error, Level*> load_level_json(const char* filename);
// MAYBE:?
//Level* load_level(const char* filename, MemoryPool mempool);
Option<Error> unload_level(Level*);


void render_tilemap(Tilemap* map);