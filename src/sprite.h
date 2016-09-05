#ifndef PLATE_SPRITE_H
#define PLATE_SPRITE_H

// Sprites contain only the metadata required to display things on screen and provide collision information.
// This data is immutable when loaded.

#include "hitbox.h"
#include "either.h"
#include "error.h"
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_rect.h>

namespace Errors {
	const error_data
		SpriteLoadJsonInvalid = { 201, "Json does not match the expected schema for Sprite" };
}

typedef struct {
	short x, y;
} FrameOffset;

typedef struct {
	const SDL_Rect* texture_region; // Points to the same data as the toplevel sprite's texture regions
	FrameOffset display; // Relative position of the upper-left corner of the texture from the origin
	FrameOffset foot; // Relative position of the "foot" coordinate which is used to follow slopes.
	int n_offsets;
	const FrameOffset* offsets; // Other offsets that might be of interest
	int n_hitboxes;
	HitboxGroup* hitbox_groups;
} Frame;

typedef struct {
	float delay;
	const Frame* frame;
} FrameTiming;

typedef struct {
	const char* name;
	int n_frames;
	const FrameTiming *frames;
} Animation;

typedef struct {
	const char* name;
	const SDL_Texture* texture;
	int n_regions; // frame regions
	const SDL_Rect* regions;
	int n_frames;
	const Frame* framedata;
	int n_animations;
	const Animation* animations;
} Sprite;

const Either<Error, const Sprite*> load_sprite_json(char* filename);

const Sprite* load_sprite(char* filename); // load from "compiled" format
void unload_sprite(Sprite* sprite); // deallocates all associated resources

void render_hitboxes(SDL_Renderer* context, SDL_Point origin, const Frame* framedata);

#endif