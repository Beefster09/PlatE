
#include "display.h"

void render_hitboxes(SDL_Renderer* context, SDL_Point origin, const Frame* framedata) {
	for (int i = 0; i < framedata->n_hitboxes; ++i) {
		SDL_Color color = get_hitbox_color(framedata->hitbox_groups[i].type);
		SDL_Rect* rects = get_hitbox_rects_relative_to(framedata->hitbox_groups[i], origin);
		SDL_SetRenderDrawColor(context, color.r, color.g, color.b, 63); // fill
		SDL_RenderFillRects(context, rects, framedata->n_hitboxes);
		SDL_SetRenderDrawColor(context, color.r, color.g, color.b, 192); // outline
		SDL_RenderDrawRects(context, rects, framedata->n_hitboxes);
		delete rects;
	}
}