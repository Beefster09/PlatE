
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
	if (strcmp(namebuf, "hurtbox") == 0) return HitboxType::HURTBOX;
	if (strcmp(namebuf, "damage") == 0) return HitboxType::DAMAGE;
	if (strcmp(namebuf, "area") == 0) return HitboxType::AREA;
	if (strcmp(namebuf, "trigger") == 0) return HitboxType::TRIGGER;
	if (strcmp(namebuf, "block") == 0) return HitboxType::BLOCK;
	return HitboxType::UNKNOWN;
}

const char* name_of(HitboxType type) {
	switch (type) {
	case HitboxType::SOLID:   return "Solid";
	case HitboxType::AREA:    return "Area";
	case HitboxType::HURTBOX: return "Hurtbox";
	case HitboxType::DAMAGE:  return "Damage";
	case HitboxType::BLOCK:   return "Block";
	case HitboxType::TRIGGER: return "Trigger";
	default: return "<???>";
	}
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

bool hitbox_types_collide(HitboxType a, HitboxType b) {
	switch (a) {
	case HitboxType::SOLID:
		return b == HitboxType::SOLID
			|| b == HitboxType::AREA
			|| b == HitboxType::TRIGGER;
	case HitboxType::AREA:
	case HitboxType::TRIGGER:
		return b == HitboxType::SOLID;
	case HitboxType::HURTBOX:
	case HitboxType::BLOCK:
		return b == HitboxType::DAMAGE;
	case HitboxType::DAMAGE:
		return b == HitboxType::HURTBOX
			|| b == HitboxType::BLOCK;
	default:
		return false;
	}
}

bool hitbox_acts_on(HitboxType a, HitboxType b) {
	switch (b) {
	case HitboxType::SOLID:
		return a == HitboxType::SOLID
			|| a == HitboxType::TRIGGER;
	case HitboxType::DAMAGE:
	case HitboxType::TRIGGER:
		return false;
	case HitboxType::HURTBOX:
	case HitboxType::BLOCK:
	case HitboxType::AREA:
	default:
		return true;
	}
}

bool hitboxes_overlap(const Hitbox* a, int ax, int ay, const Hitbox* b, int bx, int by) {
	if (a->shape == Hitbox::BOX) {
		if (b->shape == Hitbox::BOX) {
			float aLeft = ax + a->box.x;
			float aRight = aLeft + a->box.w;
			float aTop = ay + a->box.y;
			float aBottom = aTop + a->box.h;

			float bLeft = bx + b->box.x;
			float bRight = bLeft + b->box.w;
			float bTop = by + b->box.y;
			float bBottom = bTop + b->box.h;

			return (
				aLeft < bRight && aRight > bLeft &&
				aTop < bBottom && aBottom > bTop
				);
		}
		else if (b->shape == Hitbox::CIRCLE) {
			float left = ax + a->box.x;
			float right = left + a->box.w;
			float top = ay + a->box.y;
			float bottom = top + a->box.h;

			float cx = bx + b->circle.x;
			float cy = by + b->circle.y;
			float r = b->circle.radius;

			if (cx < left) {
				// center is left of box
				if (cy < top) {
					// in top-left corner
					float dx = cx - left;
					float dy = cy - top;
					return dx * dx + dy * dy < r * r;
				}
				else if (cy > bottom) {
					// in bottom-left corner
					float dx = cx - left;
					float dy = cy - bottom;
					return dx * dx + dy * dy < r * r;
				}
				else {
					// just to the left
					return cx < left - r;
				}
			}
			else if (cx > right) {
				// center is left of box
				if (cy < top) {
					// in top-right corner
					float dx = cx - right;
					float dy = cy - top;
					return dx * dx + dy * dy < r * r;
				}
				else if (cy > bottom) {
					// in bottom-right corner
					float dx = cx - right;
					float dy = cy - bottom;
					return dx * dx + dy * dy < r * r;
				}
				else {
					// just to the right
					return cx > right + r;
				}
			}
			else {
				if (cy < top) {
					// above
					return cy < top - r;
				}
				else if (cy > bottom) {
					// below
					return cy > bottom + r;
				}
				else {
					// center is inside box
					return true;
				}
			}
		}
	} else if (a->shape == Hitbox::CIRCLE) {
		if (b->shape == Hitbox::CIRCLE) {
			float dx = (ax + a->circle.x) - (bx + b->circle.x);
			float dy = (ay + a->circle.y) - (by + b->circle.y);
			float dist = a->circle.radius + b->circle.radius;

			return dx * dx + dy * dy < dist * dist;
		}
		else if (b->shape == Hitbox::BOX) {
			return hitboxes_overlap(b, bx, by, a, ax, ay);
		}
	}
	else {
		printf("WARNING: Reached default case of hitboxes_overlap(...)\n");
		return false;
	}
}
