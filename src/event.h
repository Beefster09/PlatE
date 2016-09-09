#pragma once

#include "either.h"
#include "error.h"
#include "hitbox.h"
#include <SDL2/SDL_mutex.h>

namespace Errors {
	const error_data
		EventQueueCannotCreateMutex = { -500, "SDL failed to create mutex!" },
		EventQueueCannotLockMutex = { -501, "SDL failed to lock mutex" },
		EventQueueCannotUnlockMutex = { -502, "SDL failed to unlock mutex" },
		EventQueueInitError = { 500, "Event queue could not be initialized" },
		EventQueueFull = { 501, "Event queue is full" };
}

struct entity;

enum class EventType {
	Collision
};

typedef struct event_t {
	EventType type;

	union {
		struct {
			HitboxType typeA, typeB;
			entity *entityA, *entityB;
		} collision;
	};
} Event;

// A "queue" that is either being emptied or filled, not both
typedef struct event_buffer {
	SDL_mutex* lock;
	Event* events;
	size_t capacity;
	size_t size;
	size_t next_pop;
} EventBuffer;

Either<Error, EventBuffer*> create_EventBuffer(size_t capacity);
Error destroy_EventBuffer(EventBuffer* buffer);
Error push_event(EventBuffer* buffer, Event &ev);
Either<Error, Event> pop_event(EventBuffer* buffer);
bool has_events(EventBuffer* buffer);