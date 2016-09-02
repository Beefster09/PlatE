
#include "display.h"

void render_hitboxes(SDL_Renderer* context, SDL_Point origin, const Frame& framedata) {
	SDL_Rect* rects = getAbsoluteRects(origin, framedata);

	for (int i = 0; i < framedata.n_hitboxes; ++i) {
		uint32_t color = HITBOX_COLORS[framedata.hitboxes[i].type];
		uint8_t r, g, b;
		r = (color & 0xFF0000) >> 16;
		g = (color & 0x00FF00) >> 8;
		b = (color & 0x0000FF);
		SDL_SetRenderDrawColor(context, r, g, b, 63); // fill
		SDL_RenderFillRect(context, rects + i);
		SDL_SetRenderDrawColor(context, r, g, b, 192); // outline
		SDL_RenderDrawRect(context, rects + i);
	}

	delete rects;
}