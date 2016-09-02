
#include "gameloop.h"

void update(int delta_time) {

}

void render(SDL_Renderer* context) {

}

void event(const SDL_Event& event) {

}

void exit() {
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}