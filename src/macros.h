#pragma once
#include <cmath>

#define EXIT_SUCCESS 0
#define EXIT_SDL_INIT_FAIL -1
#define EXIT_SDL_EVENT_FAIL -2
#define EXIT_SDL_GPU_FAIL -10

#define BIT(n) (1 << (n))

#define FLOAT_EPSILON 0.001f
#define FLOAT_EQ(x, y) (x >= y - FLOAT_EPSILON && x <= y + FLOAT_EPSILON)
#define FLOAT_EQ_EPSILON(x, y, epsilon) (x >= y - epsilon && x <= y + epsilon)

#define DEG_TO_RAD(x) (x * (3.1415926535f / 180.f))
#define RAD_TO_DEG(x) (x * (180.f / 3.1415926535f))