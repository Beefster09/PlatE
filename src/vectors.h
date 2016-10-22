#pragma once

#include "SDL2/SDL_rect.h"
#include "storage.h"
#include <cmath>
#include "angelscript.h"

struct Vector2 {
	float x, y;

	float dot(const Vector2& other) const;
	float cross(const Vector2& other) const;
	float magnitude() const;
	inline float angle() const { return atan2f(y, x); }
	Vector2 normalized() const;
	Vector2 projected(Vector2 axis) const;
	Vector2 rotated(float angle) const;
	Vector2 floor() const;
	inline Vector2 rotated90CW() const { return{ y, -x }; }
	inline Vector2 rotated90CCW() const { return{ -y, x }; }

	Vector2 operator + (const Vector2&) const;
	Vector2 operator - (const Vector2&) const;
	Vector2 operator * (float scalar) const;
	Vector2 operator / (float scalar) const;
	Vector2 operator - () const;

	void normalize();
	void rotate(float angle);
	void round_down();

	void operator += (const Vector2&);
	void operator -= (const Vector2&);
	void operator *= (float scalar);
	void operator /= (float scalar);

	static Vector2 fromPolar(float angle, float length);

	static const Vector2 up, down, left, right, zero;
};

inline Vector2 operator * (float scalar, const Vector2 &vec) { return vec * scalar; }
inline float distance (const Vector2 &lhs, const Vector2 &rhs) { return (rhs - lhs).magnitude(); }

typedef Vector2 Point2;

struct AABB {
	float left, right, top, bottom;
};

void aabb_to_poly(const AABB& aabb, Point2* arr);
AABB poly_to_aabb(Array<Point2> polygon);

struct Circle {
	Point2 center;
	float radius;
};

void approx_circle(Circle c, Array<Point2> polygon);

bool is_convex(Array<Point2> polygon);

struct Line {
	Vector2 p1, p2;
};

SDL_Rect to_rect(const Vector2& p1, const Vector2& p2);

AABB to_aabb(Line& line);

Vector2 lerp(const Vector2& p1, const Vector2& p2, float t);

// AngelScript hooks
void Vector2ListConstructor(float* list, Vector2* self);
void RegisterVector2(asIScriptEngine* engine);