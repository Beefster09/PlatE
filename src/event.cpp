
#include "event.h"
#include <new>

Either<Error, EventBuffer*> create_EventBuffer(size_t capacity) {
	SDL_mutex* lock = SDL_CreateMutex();
	if (lock == nullptr) {
		return Errors::EventQueueCannotCreateMutex;
	}
	try {
		Event* events = new Event[capacity];
		return new EventBuffer{
			lock,
			events,
			capacity,
			0
		};
	}
	catch (std::bad_alloc &ba) {
		return Errors::BadAlloc;
	}
}

Error destroy_EventBuffer(EventBuffer* buffer) {
	delete[] buffer->events;
	SDL_DestroyMutex(buffer->lock);
	delete buffer;
	return SUCCESS;
}

// Synchronized
Error push_event(EventBuffer* q, Event &ev) {
	printf("push_event(...)\n");
	if (SDL_LockMutex(q->lock) == 0) {
		assert(q->next_pop == 0 && "Cannot push while being popped");
		Error ret = SUCCESS;
		if (q->size >= q->capacity) {
			ret = Errors::EventQueueFull;
		}
		else {
			q->events[q->size++] = ev;
		}
		if (SDL_UnlockMutex(q->lock) == 0) {
			return ret;
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
	assert(has_events(q) && "Attempt to pop from empty EventBuffer!");
	printf("pop_event(...)\n");

	Event result = q->events[q->next_pop++];
	if (q->next_pop >= q->size) {
		q->size = 0;
		q->next_pop = 0;
	}

	return result;
}

// NOT synchronized. Handle in main thread
bool has_events(EventBuffer* q) {
	return q->size > 0 && q->size > q->next_pop;
}