
#include "gameloop.h"
#include "entity.h"

void GlobalGameLoop::update(int delta_time) {

}

void GlobalGameLoop::render(SDL_Renderer* context) {

}

void GlobalGameLoop::event(const SDL_Event& event) {

}

void Global::exit() {
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}