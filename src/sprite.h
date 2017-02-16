#ifndef PLATE_SPRITE_H
#define PLATE_SPRITE_H

// Sprites contain only the metadata required to display things on screen and provide collision information.
// This data is immutable when loaded.

#include "arrays.h"
#include "vectors.h"
#include "hitbox.h"
#include "result.h"
#include "error.h"
#include "SDL_gpu.h"
#include "assetmanager.h"

#define SPRITE_MAGIC_NUMBER "PlatEsprite"

namespace Errors {
	const error_data
		InvalidSpriteHeader = { 201, "Sprite does not begin with the string \"" SPRITE_MAGIC_NUMBER "\"" };
}

typedef Vector2 FrameOffset;

struct Frame {
	const GPU_Rect* clip; // Points to the same data as the toplevel sprite's texture regions
	FrameOffset display; // Relative position of the upper-left corner of the texture from the origin
	Array<const FrameOffset> offsets; // Other offsets that might be of interest
	Array<const Collider> colliders; // Additional collision information such as hitboxes and hurtboxes
};

struct FrameTiming {
	float delay;
	const Frame* frame;
};

struct Animation {
	const char* name;
	Array<const FrameTiming> frames;


	struct Solidity {
		Hitbox hitbox;
		float head; // top of hitbox (used for negative slopes; precalculated, not stored)
		float foot; // bottom of hitbox (used for positive slopes; precalculated, not stored)
		bool fixed; // hitbox will be scaled and translated with its entity, but will not be rotated.
	} solidity;
};

struct Sprite {
	const char* name;
	GPU_Image* texture;
	Array<const GPU_Rect> clips;
	Array<const Frame> framedata;
	Array<const Animation> animations;
};

Result<const Sprite*> load_sprite(const char* filename, const DirContext& context = DirContext()); // load from "compiled" format
Result<void> unload_sprite(Sprite* sprite); // deallocates all associated resources

Result<const Sprite*> read_referenced_sprite(FILE* stream, uint32_t len, const DirContext& context);

#endif