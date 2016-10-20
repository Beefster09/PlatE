#pragma once

#include <SDL2/SDL_render.h>
#include <SDL2/SDL_events.h>
#include "error.h"
#include "entity.h"
#include "level.h"

namespace PlatE {

	class Engine {
	private:
		EntitySystem* entity_system;
		Level* active_level;
		// Level* peripheral_level; // used for splicing levels together
		// Viewport* camera;
		// InputSystem* input;
		// AssetManager asset_manager;

		// ASEngine* script_engine;
		// DenseBucket<asIContext*> context_pool;
	public:
		Either<Error, EntitySystem*> init_entity_system(size_t size = ENTITY_SYSTEM_DEFAULT_SIZE);

		void update(int delta_time);
		void render(SDL_Renderer* context);
		void event(const SDL_Event& event);

		Engine();
	};

	void exit();
}