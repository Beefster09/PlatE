
#include "vectors.h"

Vector2 Vector2::operator + (const Vector2 &that) const {
	return{ x + that.x, y + that.y };
}

Vector2 Vector2::operator - (const Vector2 &that) const {
	return{ x - that.x, y - that.y };
}

Vector2 Vector2::operator * (float scalar) const {
	return{ x * scalar, y * scalar };
}

Vector2 Vector2::operator / (float scalar) const {
	return{ x / scalar, y / scalar };
}

void Vector2::operator += (const Vector2& other) {
	x += other.x; y += other.y;
}

void Vector2::operator -= (const Vector2& other) {
	x -= other.x; y -= other.y;
}

void Vector2::operator *= (float scalar) {
	x *= scalar;
	y *= scalar;
}

void Vector2::operator /= (float scalar) {
	x /= scalar;
	y /= scalar;
}

Vector2 Vector2::operator - () const {
	return{ -x, -y };
}

float Vector2::magnitude() const {
	return sqrtf(x * x + y * y);
}

Vector2 Vector2::normalized() const {
	return operator / (magnitude());
}

void Vector2::normalize() {
	return operator /= (magnitude());
}

float Vector2::dot(const Vector2& other) const {
	return x * other.x + y * other.y;
}

// This is equivalent to a dot b.rotated90CW
float Vector2::cross(const Vector2& other) const {
	return x * other.y - y * other.x;
}

Vector2 Vector2::projected(Vector2 axis) const {
	axis.normalize();
	return dot(axis) * axis;
}

Vector2 Vector2::rotated(float angle) const {
	float s = sinf(angle);
	float c = cosf(angle);
	return{
		x * c - y * s,
		x * s + y * c
	};
}

void Vector2::rotate(float angle) {
	float s = sinf(angle);
	float c = cosf(angle);
	float x = this->x;
	float y = this->y;
	this->x = x * c - y * s;
	this->y = x * s + y * c;
}

const Vector2 Vector2::up = { 0.f, -1.f };
const Vector2 Vector2::down = { 0.f, 1.f };
const Vector2 Vector2::left = { -1.f, 0.f };
const Vector2 Vector2::right = { 1.f, 0.f };
const Vector2 Vector2::zero = { 0.f, 0.f };

// starts at right, rotates clockwise
Vector2 Vector2::fromPolar(float angle, float length) {
	return{ length * cosf(angle), length * sinf(angle) };
}