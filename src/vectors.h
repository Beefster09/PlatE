#pragma once

#include <cmath>

struct Vector2;

struct Point2 {
	float x, y;

	void operator += (const Vector2&);
	void operator -= (const Vector2&);
};

Point2 operator + (const Point2 &pt, const Vector2 &vec);
Point2 operator - (const Point2 &pt, const Vector2 &vec);
Vector2 operator - (const Point2 &a, const Point2 &b);

struct Vector2 {
	float x, y;

	float dot(const Vector2& other) const;
	float cross(const Vector2& other) const;
	float magnitude() const;
	inline float angle() const { return atan2f(y, x); }
	Vector2 normalized() const;
	Vector2 projected(Vector2 axis) const;
	Vector2 rotated(float angle) const;
	inline Vector2 rotated90CW() const { return{ y, -x }; }
	inline Vector2 rotated90CCW() const { return{ -y, x }; }

	Vector2 operator + (const Vector2&) const;
	Vector2 operator - (const Vector2&) const;
	Vector2 operator * (float scalar) const;
	Vector2 operator / (float scalar) const;
	Vector2 operator - () const;

	void normalize();
	void rotate(float angle);

	void operator += (const Vector2&);
	void operator -= (const Vector2&);
	void operator *= (float scalar);
	void operator /= (float scalar);

	static Vector2 fromPolar(float angle, float length);

	static const Vector2 up, down, left, right, zero;
};

__forceinline Vector2 operator * (float scalar, const Vector2 &vec) { return vec * scalar; }