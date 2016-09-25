
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
	int i = 0;
	for (const Hitbox& hitbox : hitboxes.hitboxes) {
		rects[i++] = {
			origin.x + hitbox.box.x,
			origin.y + hitbox.box.y,
			hitbox.box.w,
			hitbox.box.h
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

void render_hitboxes(SDL_Renderer* context, SDL_Point origin, const CollisionData& collision) {
	SDL_Rect rects[32];
	for (const HitboxGroup& group: collision.groups) {
		SDL_Color color = get_hitbox_color(group.type);
		get_hitbox_rects_relative_to(rects, group, origin);
		SDL_SetRenderDrawColor(context, color.r, color.g, color.b, 63); // fill
		SDL_RenderFillRects(context, rects, group.hitboxes.size());
		SDL_SetRenderDrawColor(context, color.r, color.g, color.b, 192); // outline
		SDL_RenderDrawRects(context, rects, group.hitboxes.size());
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

bool hitboxes_overlap(const Hitbox* a, const Point2& apos, float arot, const Hitbox* b, const Point2& bpos, float brot) {
	if (a->shape == Hitbox::ONEWAY) {
		if (b->shape == Hitbox::ONEWAY) return false; // One-ways don't collide
		else return hitboxes_overlap(b, bpos, brot, a, apos, arot); // And they can only be acted upon
	}
	if (a->shape == Hitbox::BOX) {
		if (b->shape == Hitbox::BOX) {

			float aLeft = apos.x + a->box.x;
			float aRight = aLeft + a->box.w;
			float aTop = apos.y + a->box.y;
			float aBottom = aTop + a->box.h;

			float bLeft = bpos.x + b->box.x;
			float bRight = bLeft + b->box.w;
			float bTop = bpos.y + b->box.y;
			float bBottom = bTop + b->box.h;

			return (
				aLeft < bRight && aRight > bLeft &&
				aTop < bBottom && aBottom > bTop
				);
		}
		else if (b->shape == Hitbox::CIRCLE) {
			float left = apos.x + a->box.x;
			float right = left + a->box.w;
			float top = apos.y + a->box.y;
			float bottom = top + a->box.h;

			float cx = bpos.x + b->circle.x;
			float cy = bpos.y + b->circle.y;
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
		else if (b->shape == Hitbox::LINE || b->shape == Hitbox::ONEWAY) {

		}
	}
	else if (a->shape == Hitbox::CIRCLE) {
		if (b->shape == Hitbox::CIRCLE) {
			float dx = (apos.x + a->circle.x) - (bpos.x + b->circle.x);
			float dy = (apos.y + a->circle.y) - (bpos.y + b->circle.y);
			float dist = a->circle.radius + b->circle.radius;

			return dx * dx + dy * dy < dist * dist;
		}
		else if (b->shape == Hitbox::LINE || b->shape == Hitbox::ONEWAY) {
			// distance from point to a line
			Vector2 parallel = {
				(float) b->line.x2 - b->line.x1,
				(float) b->line.y2 - b->line.y1
			};
			float parlen = parallel.magnitude();
			parallel /= parlen;
			Vector2 to_circle = {
				(apos.x + a->circle.x) - (bpos.x + b->line.x1),
				(apos.y + a->circle.y) - (bpos.y + b->line.y1)
			};
			// project center onto line
			float projpos = to_circle.dot(parallel);
			if (projpos < 0) { // outside the line on the first point side
				float dx = (apos.x + a->circle.x) - (bpos.x + b->line.x1);
				float dy = (apos.y + a->circle.y) - (bpos.y + b->line.y1);
				// distance to endpoint
				return dx * dx + dy * dy < b->circle.radius * b->circle.radius;
			}
			else if (projpos > parlen) { // outside the line on the second point side
				float dx = (apos.x + a->circle.x) - (bpos.x + b->line.x2);
				float dy = (apos.y + a->circle.y) - (bpos.y + b->line.y2);
				// distance to endpoint
				return dx * dx + dy * dy < b->circle.radius * b->circle.radius;
			}
			// TODO: velocity is needed here for oneway collisions
			return fabs(to_circle.cross(parallel)) < a->circle.radius;
		}
		else if (b->shape == Hitbox::BOX) {
			return hitboxes_overlap(b, bpos, brot, a, apos, arot);
		}
	}
	else if (a->shape == Hitbox::LINE) {
		if (b->shape == Hitbox::LINE) {
			// line intersection test
		}
		else return hitboxes_overlap(b, bpos, brot, a, apos, arot);
	}
	printf("WARNING: Reached default case of hitboxes_overlap(...)\n");
	return false;
}
