
#include "sprite.h"
#include "hitbox.h"
#include <SDL2/SDL_rect.h>
#include <cstring>
#include <cassert>

HitboxType hitbox_type_by_name(const char* name) {
	char namebuf[32];
	if (strlen(name) >= 31) return HitboxType::UNKNOWN;
	strcpy(namebuf, name);
	strlwr(namebuf);

	if (strcmp(namebuf, "solid") == 0) return HitboxType::SOLID;
	if (strcmp(namebuf, "oneway") == 0) return HitboxType::ONEWAY;
	if (strcmp(namebuf, "hurtbox") == 0) return HitboxType::HURTBOX;
	if (strcmp(namebuf, "damage") == 0) return HitboxType::DAMAGE;
	if (strcmp(namebuf, "area") == 0) return HitboxType::AREA;
	if (strcmp(namebuf, "trigger") == 0) return HitboxType::TRIGGER;
	if (strcmp(namebuf, "block") == 0) return HitboxType::BLOCK;
	return HitboxType::UNKNOWN;
}

void get_hitbox_rects_relative_to(SDL_Rect* rects, const HitboxGroup& hitboxes, SDL_Point origin) {
	for (int i = 0; i < hitboxes.n_hitboxes; ++i) {
		rects[i] = {
			origin.x + hitboxes.hitboxes[i].box.x,
			origin.y + hitboxes.hitboxes[i].box.y,
			hitboxes.hitboxes[i].box.w,
			hitboxes.hitboxes[i].box.h
		};
	}
}

SDL_Color get_hitbox_color(HitboxType type, int flags) {
	switch (type) {
		case HitboxType::SOLID:   return { 160, 160, 160, 255 };
		case HitboxType::ONEWAY:  return { 100, 100, 100, 255 };
		case HitboxType::AREA:    return {   0,  80, 255, 255 };
		case HitboxType::HURTBOX: return { 255, 255,   0, 255 };
		case HitboxType::DAMAGE:  return { 255,   0,   0, 255 };
		case HitboxType::BLOCK:   return { 160,   0, 255, 255 };
		case HitboxType::TRIGGER: return {   0, 255,   0, 255 };
		default: return { 0, 0, 0, 0 };
	}
}

void render_hitboxes(SDL_Renderer* context, SDL_Point origin, const Frame* framedata) {
	SDL_Rect rects[32];
	for (int i = 0; i < framedata->n_hitboxes; ++i) {
		SDL_Color color = get_hitbox_color(framedata->hitbox_groups[i].type);
		get_hitbox_rects_relative_to(rects, framedata->hitbox_groups[i], origin);
		SDL_SetRenderDrawColor(context, color.r, color.g, color.b, 63); // fill
		SDL_RenderFillRects(context, rects, framedata->n_hitboxes);
		SDL_SetRenderDrawColor(context, color.r, color.g, color.b, 192); // outline
		SDL_RenderDrawRects(context, rects, framedata->n_hitboxes);
	}
}
