#pragma once

#include <SDL2/SDL_render.h>
#include <SDL2/SDL_events.h>

void update(int delta_time);
void render(SDL_Renderer* context);
void event(const SDL_Event& event);

void exit();