#pragma once

#include <cmath>

struct Vector2;

struct Point2 {
	float x, y;

	void operator += (const Vector2);
	void operator -= (const Vector2);
};

Point2 operator + (const Point2 &pt, const Vector2 &vec);
Point2 operator - (const Point2 &pt, const Vector2 &vec);
Vector2 operator - (const Point2 &a, const Point2 &b);

struct Vector2 {
	float x, y;

	float dot(const Vector2& other) const;
	float cross(const Vector2& other) const;
	float magnitude() const;
	inline float angle() const { return atan2(y, x); }
	Vector2 normal() const;
	void normalize();
};

Vector2 operator + (const Vector2 &a, const Vector2 &b);
Vector2 operator - (const Vector2 &a, const Vector2 &b);
Vector2 operator * (float scalar, const Vector2 &vec);
Vector2 operator - (const Vector2 &vec);
inline Vector2 operator * (const Vector2 &vec, float scalar) { return scalar * vec; }