#pragma once

#include "either.h"
#include "error.h"
#include "hitbox.h"
#include <SDL2/SDL_mutex.h>
#include <deque>

namespace Errors {
	const error_data
		EventQueueCannotCreateMutex = { -500, "SDL failed to create mutex!" },
		EventQueueCannotLockMutex = { -501, "SDL failed to lock mutex" },
		EventQueueCannotUnlockMutex = { -502, "SDL failed to unlock mutex" },
		EventQueueInitError = { 500, "Event queue could not be initialized" },
		EventQueueFull = { 501, "Event queue is full" },
		EventQueueEmpty = { 502, "Event queue is empty" };
}

struct Entity;

struct Event {
	enum : char {
		Collision
	} type;

	union {
		struct {
			const Collider *hitboxA, *hitboxB;
			Entity *entityA, *entityB;
		} collision;
	};
};

struct EventBuffer {
	SDL_mutex* lock;
	std::deque<Event> events;
};

Result<EventBuffer*> create_EventBuffer();
Result<void> destroy_EventBuffer(EventBuffer* buffer);
Result<void> push_event(EventBuffer* buffer, Event &ev);
Result<Event> pop_event(EventBuffer* buffer);
bool has_events(EventBuffer* buffer);