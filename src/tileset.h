#pragma once

#include "storage.h"
#include "hitbox.h"
#include "assetmanager.h"
#include "SDL2\SDL_render.h"

#define TILESET_MAGIC_NUMBER "PlatEtileset"
#define TILESET_MAGIC_NUMBER_LENGTH (sizeof(TILESET_MAGIC_NUMBER) - 1)

namespace Errors {
	const error_data
		InvalidTilesetHeader = { 201, "Tileset does not begin with the string \"" TILESET_MAGIC_NUMBER "\"" };
}

// tile 0 is reserved as a blank tile
#define TILE_BLANK 0

struct TileFrame {
	uint16_t x_ind, y_ind;
	float duration;
	uint8_t flip;
};

struct TileProperty {
	const char* name;
	//TODO: room for some data... Hmmm...
};

struct Tile {
	Array<const TileFrame> animation;

	// tile properties that might be of interest to scripts
	Array<const TileProperty> properties;

	// Solidity that matters to physics engine
	struct Solidity {
		enum Type : char {
			None      =  0,  // No solidity whatsoever
			Full      = 'F', // Tile is completely solid
			Partial   = 'P', // Uses horizontal or vertical dividing line where one side is solid
			Slope     = 'S', // Uses sloped dividing line where one side is solid
			                 //   (simpler than polygons, less penetrable than lines)
			Complex   = 'C'  // Uses hitbox
		} type;

		union {
			struct {
				float position; // y value if horizontal, x if vertical
				bool vertical;
				bool topleft; // true if the topleft corner is solid
			} partial;
			struct {
				float position; // y intercept
				float slope;
				bool above; // true if the area above the line is solid
			} slope;
			Hitbox complex;
		};
	} solidity;
};

struct Tileset {
	const char* name;
	GPU_Image* tilesheet;
	uint16_t tile_width, tile_height;
	Array<const Tile> tile_data; // 1 indexed; tile 0 is reserved as a blank tile
	// Later?
	// ShaderType shader;
};

struct TileAnimationState {
	const Tile* data;
	float frame_time;
	uint16_t anim_frame;
};

Result<const Tileset*> load_tileset(const char* filename, const DirContext& context = DirContext());

Result<> unload_tileset(const Tileset*);

Result<const Tileset*> read_referenced_tileset(FILE* stream, uint32_t len, const DirContext& context);