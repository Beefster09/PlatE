#ifndef PLATE_SPRITE_H
#define PLATE_SPRITE_H

// Sprites contain only the metadata required to display things on screen and provide collision information.
// This data is immutable when loaded.

#include "storage.h"
#include "vectors.h"
#include "hitbox.h"
#include "either.h"
#include "error.h"
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_rect.h>

#define SPRITE_MAGIC_NUMBER "PlatEsprite"
#define SPRITE_MAGIC_NUMBER_LENGTH (sizeof(SPRITE_MAGIC_NUMBER) - 1)
// 1 MB sprite data limit
#define SPRITE_MAX_SIZE (1024 * 1024)

namespace Errors {
	const error_data
		InvalidSpriteHeader = { 201, "Sprite does not begin with the string \"" SPRITE_MAGIC_NUMBER "\"" },
		SpriteDataTooLarge = { 202, "Sprites are limited to be 1 MB (not including texture)" };
}

typedef Vector2 FrameOffset;

struct Frame {
	const SDL_Rect* clip; // Points to the same data as the toplevel sprite's texture regions
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
	Array<const SDL_Rect> clips;
	Array<const Frame> framedata;
	Array<const Animation> animations;
};

Either<Error, const Sprite*> load_sprite(const char* filename); // load from "compiled" format
void unload_sprite(Sprite* sprite); // deallocates all associated resources

#endif