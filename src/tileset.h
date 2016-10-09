#pragma once

#include "storage.h"
#include "hitbox.h"
#include "SDL2\SDL_render.h"

struct TileFrame {
	unsigned int index;
	float time;
};

struct Tile {
	Array<TileFrame> animation;
	CollisionData collision;
};

struct Tileset {
	SDL_Texture* texture;
	unsigned int tileW, tileH;
	unsigned int n_tilesW, n_tilesH;
	Array<Tile> tile_data;
};


Either<Error, Tileset*> load_tileset_json(const char* filename);
Either<Error, Tileset*> load_tileset(const char* filename);

Option<Error> unload_tileset(Tileset*);