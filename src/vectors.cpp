
#include "vectors.h"

Point2 operator + (const Point2 &pt, const Vector2 &vec) {
	return{ pt.x + vec.x, pt.y + vec.y };
}

Point2 operator - (const Point2 &pt, const Vector2 &vec) {
	return{ pt.x - vec.x, pt.y - vec.y };
}

Vector2 operator - (const Point2 &a, const Point2 &b) {
	return{ a.x - b.x, a.y - b.y };
}

Vector2 operator + (const Vector2 &a, const Vector2 &b) {
	return{ a.x + b.x, a.y + b.y };
}

Vector2 operator - (const Vector2 &a, const Vector2 &b) {
	return{ a.x - b.x, a.y - b.y };
}

Vector2 operator * (float scalar, const Vector2 &vec) {
	return{ scalar * vec.x, scalar * vec.y };
}

float Vector2::magnitude() const {
	return sqrt(x * x + y * y);
}

Vector2 operator - (const Vector2 &vec) {
	return{ -vec.x, -vec.y };
}

float Vector2::dot(const Vector2& other) const {
	return x * other.x + y * other.y;
}

float Vector2::cross(const Vector2& other) const {
	return x * other.y - y * other.x;
}