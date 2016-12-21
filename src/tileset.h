#pragma once

#include "storage.h"
#include "hitbox.h"
#include "SDL2\SDL_render.h"

// tile 0 is reserved as a blank tile
#define TILE_BLANK 0

struct TileFrame {
	uint16_t x_ind, y_ind;
	float time;
};

struct Tile {
	Array<TileFrame> animation;
	CollisionData collision;
};

struct Tileset {
	SDL_Texture* texture;
	uint16_t tileW, tileH;
	uint16_t n_tilesW, n_tilesH;
	Array<Tile> tile_data; // 1 indexed; tile 0 is reserved as a blank tile
};


Result<Tileset*> load_tileset_json(const char* filename);
Result<Tileset*> load_tileset(const char* filename);

Option<Error> unload_tileset(Tileset*);