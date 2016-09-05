
#include "gameloop.h"
#include "entity.h"

using namespace PlatE;

Either<Error, EntitySystem*> Engine::add_entity_system(size_t capacity) {
	auto result = create_entity_system(capacity);

	if (result.isLeft) {
		return result.left;
	}
	else {
		entitySystems.push_back(result.right);
		return result.right;
	}
}

void Engine::update(int delta_time) {
	for (auto system : entitySystems) {
		process_entities(delta_time, system);
	}
}

void Engine::render(SDL_Renderer* context) {
	for (auto system : entitySystems) {
		render_entities(context, system);
	}

}

void Engine::event(const SDL_Event& event) {

}

void PlatE::exit() {
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}