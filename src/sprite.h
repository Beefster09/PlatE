#ifndef PLATE_SPRITE_H
#define PLATE_SPRITE_H

// Sprites contain only the metadata required to display things on screen and provide collision information.
// This data is immutable when loaded.

#include "storage.h"
#include "hitbox.h"
#include "either.h"
#include "error.h"
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_rect.h>

namespace Errors {
	const error_data
		SpriteLoadJsonInvalid = { 201, "Json does not match the expected schema for Sprite" };
}

struct FrameOffset {
	int x, y;
};

struct Frame {
	const SDL_Rect* texture_region; // Points to the same data as the toplevel sprite's texture regions
	FrameOffset display; // Relative position of the upper-left corner of the texture from the origin
	FrameOffset foot; // Relative position of the "foot" coordinate which is used to follow slopes.
	Array<const FrameOffset> offsets; // Other offsets that might be of interest
	CollisionData collision;
};

struct FrameTiming {
	float delay;
	const Frame* frame;
};

struct Animation {
	const char* name;
	Array<const FrameTiming> frames;
};

struct Sprite {
	const char* name;
	const SDL_Texture* texture;
	Array<const SDL_Rect> regions;
	Array<const Frame> framedata;
	Array<const Animation> animations;
};

const Either<Error, const Sprite*> load_sprite_json(char* filename);

const Sprite* load_sprite(char* filename); // load from "compiled" format
void unload_sprite(Sprite* sprite); // deallocates all associated resources

#endif