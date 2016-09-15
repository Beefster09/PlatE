#pragma once

#include <SDL2/SDL_render.h>
#include <SDL2/SDL_events.h>
#include <vector>
#include "error.h"
#include "entity.h"

namespace PlatE {

	class Engine {
	private:
		std::vector<EntitySystem*> entitySystems;
	public:
		Either<Error, EntitySystem*> add_entity_system(size_t size = ENTITY_SYSTEM_DEFAULT_SIZE);

		void update(int delta_time);
		void render(SDL_Renderer* context);
		void event(const SDL_Event& event);
	};

	void exit();
}