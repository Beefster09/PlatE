#pragma once
#include <cmath>
#include <type_traits>

// Constants and macros

#define BIT(n) (1 << (n))

constexpr float FLOAT_EPSILON = 0.001f;

// Utility functions to be inlined - typically quick calculations that could also be done as macros, but this way is safer

__forceinline bool float_eq(const float x, const float y, const float epsilon = FLOAT_EPSILON) {
	return (x >= y - epsilon && x <= y + epsilon);
}

__forceinline float deg_to_rad(const float x) { return x * (3.1415926535f / 180.f); }
__forceinline float rad_to_deg(const float x) { return x * (180.f / 3.1415926535f); }

/// checks if value is within factor orders of 2 magnitude of park
template <class T>
__forceinline bool ballpark(const T value, const T park, const int factor = 1) {
	static_assert(std::is_arithmetic<T>::value, "ballpark(value, park, factor) requires numeric type");

	// yay for metaprogramming and dead code removal
	if (std::is_integral<T>::value) {
		return value >= (park >> factor) && value <= (park << factor);
	}
	else {
		return value >= ldexp(park, factor) && value <= ldexp(park, -factor);
	}
}

inline char* copy(const char* str) {
	char* ret = new char[strlen(str) + 1];
	strcpy(ret, str);
	return ret;
}

template <class T>
inline T clamp(const T value, const T min, const T max) {
	static_assert(std::is_arithmetic<T>::value, "clamp(value, min, max) requires numeric type");

	if (value < min) return min;
	if (value > max) return max;
	return value;
}