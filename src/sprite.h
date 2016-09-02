#ifndef PLATE_SPRITE_H
#define PLATE_SPRITE_H

// Sprites contain only the metadata required to display things on screen and provide collision information.
// This data is intended to be immutable.

#include "hitbox.h"
#include "SDL2\SDL_rect.h"

typedef struct {
	short x, y;
} FrameOffset;

typedef struct {
	unsigned short textureX, textureY, textureW, textureH; // Bounding box to display from the texture
	FrameOffset display; // Relative position of the upper-left corner of the texture from the origin
	FrameOffset foot; // Relative position of the "foot" coordinate which is used to follow slopes.
	int n_offsets;
	FrameOffset* offsets; // Other offsets that might be of interest
	int n_hitboxes;
	Hitbox* hitboxes;
} Frame;

typedef struct {
	char* name;
	int n_frames;
	Frame* frames;
} Animation;

typedef struct {
	char* name;
	void* texture;
	int n_animations;
	Animation* animations;
} Sprite;

SDL_Rect* getAbsoluteRects(SDL_Point, const Frame&);

#endif