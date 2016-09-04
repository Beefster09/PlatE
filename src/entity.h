#pragma once

#include <cstdint>
#include <cmath>
#include <cassert>
#include "sprite.h"
#include "hitbox.h"
#include "either.h"

namespace Errors {
	error_data
		EntitySystemCapacityReached = { 300, "Entity system has reached its maximum entity count" };
}

/** Pixel-perfect coordinate component
 * It's a class purely for implicit conversions and += operators
*/
class PixelUnit {
public:
	int32_t pixel;
private:
	float subpixel; // must be private to keep within [0, 1)
	inline void normalize() { // Essentially just a hygenic macro
		float extra = floor(subpixel);
		pixel += extra;
		subpixel -= extra;
		assert(subpixel >= 0.0 && subpixel < 1.0);
	}

public:
	inline PixelUnit() { pixel = 0; subpixel = 0.0f; }
	inline PixelUnit(const PixelUnit& copy) { pixel = copy.pixel; subpixel = copy.subpixel; }

	// Implicit conversions
	inline PixelUnit(double d) { pixel = (int)floor(d); subpixel = d - pixel; }
	inline PixelUnit(float f) { pixel = (int)floor(f); subpixel = f - pixel; }
	inline PixelUnit(int pix) { pixel = pix; subpixel = 0.0f; }

	inline operator int() const { return pixel; }
	inline operator double() const { return (double)subpixel + pixel; }
	inline operator float() const { return subpixel + pixel; }

	// Operators
	inline void operator += (const PixelUnit& other) {
		pixel += other.pixel;
		subpixel += other.subpixel;
		normalize();
	}
	inline void operator -= (const PixelUnit& other) {
		pixel -= other.pixel;
		subpixel -= other.subpixel;
		normalize();
	}

	inline void operator += (int other) { pixel += other; }
	inline void operator -= (int other) { pixel -= other; }

	inline friend PixelUnit& operator + (const PixelUnit& lhs, const PixelUnit& rhs) {
		PixelUnit result = lhs;
		result += rhs;
		return result;
	}

	inline friend PixelUnit& operator - (const PixelUnit& lhs, const PixelUnit& rhs) {
		PixelUnit result = lhs;
		result -= rhs;
		return result;
	}

	inline friend PixelUnit& operator + (const PixelUnit& lhs, int rhs) {
		PixelUnit result = lhs;
		result += rhs;
		return result;
	}

	inline friend PixelUnit& operator + (int lhs, const PixelUnit& rhs) { return rhs + lhs; }

	// Note that there are no * or / operators. This is not a mistake.
	// PixelUnits should only ever be translated.
};

typedef struct entity {
	size_t index;
	uint32_t id;

	struct {
		PixelUnit x, y;
	} position;

	struct {
		float x, y;
	} velocity;

	struct { // Not sure if this is how I should do gravity...
		float x, y;
	} acceleration;

	const Sprite* sprite;
	const Animation* curAnimation;
	uint32_t curAnimFrame;
	float curFrameTime;
	const Frame* curFrame;

	// Scripting / Events
	/*
	void* script; // not sure what type this should be...
	int32_t scriptSlots[16];
	void* callbacks; // event callback struct (LATER)
	*/
} Entity;

typedef struct entity_system {
	Entity* entities;

	uint32_t nextId;
	size_t n_entities;
	size_t max_entities;
} EntitySystem;

Either<Error, EntitySystem> create_entity_system(size_t capacity);
Error dispose_entity_system(EntitySystem& system);

Error add_entity(EntitySystem& system, const Entity& entity);
Error remove_entity(EntitySystem& system, uint32_t id);

// TODO/later: allow interactions across multiple entity systems and level data
// Things to think about:
//  * Is this where physics/collision detection are calculated?
//    - Or is it done on another pass?
//  * Are events processed at this time?
//    - Are events synchronous or asynchronous (which would require some sort of queue structure...)
//    - Is the events queue shared or individual?
//  * How is this supposed to interact with players?
void process_entities(EntitySystem& system);

// self-explanatory
void render_entities(SDL_Renderer* context, const EntitySystem& system);