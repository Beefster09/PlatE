
#include "event.h"
#include <new>

Either<Error, EventBuffer*> create_EventBuffer() {
	SDL_mutex* lock = SDL_CreateMutex();
	if (lock == nullptr) {
		return Errors::EventQueueCannotCreateMutex;
	}
	return new EventBuffer {
		lock,
		std::deque<Event>()
	};
}

Error destroy_EventBuffer(EventBuffer* buffer) {
	SDL_DestroyMutex(buffer->lock);
	delete buffer; // Cleans up deque because RAII
	return SUCCESS;
}

// Synchronized
Error push_event(EventBuffer* q, Event &ev) {
	if (SDL_LockMutex(q->lock) == 0) {
		q->events.push_back(ev);
		if (SDL_UnlockMutex(q->lock) == 0) {
			return SUCCESS;
		}
		else {
			return Errors::EventQueueCannotUnlockMutex;
		}
	}
	else {
		return Errors::EventQueueCannotLockMutex;
	}
}


// NOT synchronized. Should be handled in sequence.
Either<Error, Event> pop_event(EventBuffer* q) {
	// Check and return error to avoid throwing an exception
	if (!has_events(q)) return Errors::EventQueueEmpty;
	// No-throw guarantee!
	Event ev = q->events.front();
	q->events.pop_front();
	return ev;
}

// NOT synchronized. Handle in main thread
bool has_events(EventBuffer* q) {
	return q->events.size() > 0;
}