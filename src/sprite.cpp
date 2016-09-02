
#include "sprite.h"

SDL_Rect* getAbsoluteRects (SDL_Point origin, const Frame& framedata) {
	SDL_Rect* result = new SDL_Rect[framedata.n_hitboxes];

	for (int i = 0; i < framedata.n_hitboxes; ++i) {
		result[i].x = origin.x + framedata.hitboxes[i].box.x;
		result[i].y = origin.y + framedata.hitboxes[i].box.y;
		result[i].w = framedata.hitboxes[i].box.w;
		result[i].h = framedata.hitboxes[i].box.h;
	}

	return result;
}