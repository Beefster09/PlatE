
#include "engine.h"
#include "entity.h"

using namespace PlatE;

Engine::Engine() {
	entity_system = nullptr;

	// TODO: init Scripting API
}

void Engine::update(int delta_time) {
	process_entities(delta_time, entity_system);
}

void Engine::render(SDL_Renderer* context) {
	render_entities(context, entity_system);
}

void Engine::event(const SDL_Event& event) {

}

void PlatE::exit() {
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

Either<Error, EntitySystem*> Engine::init_entity_system(size_t capacity) {
	auto result = create_entity_system(capacity);

	if (result.isLeft) {
		return result.left;
	}
	else {
		if (entity_system != nullptr) {
			auto res = destroy_entity_system(entity_system);
			if (res) {
				return res.value;
			}
		}

		entity_system = result.right;
		return result.right;
	}
}