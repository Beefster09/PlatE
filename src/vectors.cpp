
#include "vectors.h"
#include "SDL_gpu.h"
#include "util.h"
#include <string>

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

Vector2 Vector2::clamped(const AABB& box) const {
	Vector2 newvec;

	if (x < box.left) newvec.x = box.left;
	else if (x > box.right) newvec.x = box.right;
	else newvec.x = x;

	if (y < box.top) newvec.y = box.top;
	else if (y > box.bottom) newvec.y = box.bottom;
	else newvec.y = y;

	return newvec;
}

void Vector2::clamp(const AABB& box) {
	if (x < box.left) x = box.left;
	else if (x > box.right) x = box.right;

	if (y < box.top) y = box.top;
	else if (y > box.bottom) y = box.bottom;
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

Vector2 Vector2::floor() const {
	return{ floorf(x), floorf(y) };
}

void Vector2::round_down() {
	x = floorf(x);
	y = floorf(y);
}

// AABB

AABB AABB::operator | (const AABB& other) const {
	return{
		SDL_min(left, other.left),
		SDL_max(right, other.right),
		SDL_min(top, other.top),
		SDL_max(bottom, other.bottom)
	};
}

AABB AABB::operator & (const AABB& other) const {
	return{
		SDL_max(left, other.left),
		SDL_min(right, other.right),
		SDL_max(top, other.top),
		SDL_min(bottom, other.bottom)
	};
}

AABB AABB::operator + (const Vector2& vec) const {
	return{
		left + vec.x,
		right + vec.x,
		top + vec.y,
		bottom + vec.y
	};
}

AABB AABB::operator - (const Vector2& vec) const {
	return{
		left - vec.x,
		right - vec.x,
		top - vec.y,
		bottom - vec.y
	};
}

void AABB::operator += (const Vector2& vec) {
	left += vec.x;
	right += vec.x;
	top += vec.y;
	bottom += vec.y;
}

void AABB::operator -= (const Vector2& vec) {
	left -= vec.x;
	right -= vec.x;
	top -= vec.y;
	bottom -= vec.y;
}

// utilities

GPU_Rect to_rect(const Vector2& p1, const Vector2& p2) {
	return {
		SDL_min(p1.x, p2.x),
		SDL_max(p1.y, p2.y),
		fabsf(p2.x - p1.x),
		fabsf(p2.y - p1.y)
	};
}

void aabb_to_poly(const AABB& aabb, Point2* arr) {
	arr[0] = { aabb.left, aabb.top };
	arr[1] = { aabb.right, aabb.top };
	arr[2] = { aabb.right, aabb.bottom };
	arr[3] = { aabb.left, aabb.bottom };
}

Vector2 lerp(const Vector2& p1, const Vector2& p2, float t) {
	return p1 + (p2 - p1) * t;
}

Vector2 ease(const Vector2& p1, const Vector2& p2, float t) {
	return p1 + ((p2 - p1) * (1.f - cosf(t * M_PI)) / 2.f);
}

float lerp(float a, float b, float t) {
	return a + (b - a) * t;
}

float ease(float a, float b, float t) {
	return a + ((b - a) * (1.f - cosf(t * M_PI)) / 2.f);
}

// ==== SCRIPTING INTERFACE ====

std::string to_string(const Vector2& vec) {
	char buf[100];
	sprintf(buf, "(%g, %g)", vec.x, vec.y);
	return std::string(buf);
}

void PrintLn(const Vector2& vec) {
	printf("(%g, %g)\n", vec.x, vec.y);
}

void Vector2ListConstructor(float* list, Vector2* self) {
	new(self) Vector2{ list[0], list[1] };
}

void Vector2Constructor(float x, float y, void* memory) {
	new(memory) Vector2{ x, y };
}

void RegisterVector2(asIScriptEngine* engine) {
#define check(EXPR) do {int r = (EXPR); assert(r >= 0);} while (0)
	check(engine->SetDefaultNamespace(""));

	// Types
	check(engine->RegisterObjectType("Vector2", sizeof(Vector2),
		asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<Vector2>() | asOBJ_APP_CLASS_ALLFLOATS));
	check(engine->RegisterObjectType("AABB", sizeof(AABB),
		asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<AABB>() | asOBJ_APP_CLASS_ALLFLOATS));

	// === Vector2 Interface ===
	// Initializers
	check(engine->RegisterObjectBehaviour("Vector2", asBEHAVE_LIST_CONSTRUCT, "void f(const int& in) {float, float}",
		asFUNCTION(Vector2ListConstructor), asCALL_CDECL_OBJLAST));
	check(engine->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT, "void f(float, float)",
		asFUNCTION(Vector2Constructor), asCALL_CDECL_OBJLAST));

	// Properties
	check(engine->RegisterObjectProperty("Vector2", "float x", asOFFSET(Vector2, x)));
	check(engine->RegisterObjectProperty("Vector2", "float y", asOFFSET(Vector2, y)));

	// Compound assignment operators
	check(engine->RegisterObjectMethod("Vector2", "Vector2 opAddAssign(const Vector2 &in)",
		asMETHODPR(Vector2, operator+=, (const Vector2&), void), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 opSubAssign(const Vector2 &in)",
		asMETHODPR(Vector2, operator-=, (const Vector2&), void), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 opMulAssign(float scalar)",
		asMETHODPR(Vector2, operator*=, (float), void), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 opDivAssign(float scalar)",
		asMETHODPR(Vector2, operator/=, (float), void), asCALL_THISCALL));

	// Const operators
	check(engine->RegisterObjectMethod("Vector2", "Vector2 opAdd(Vector2 &in) const",
		asMETHODPR(Vector2, operator+, (const Vector2&) const, Vector2), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 opSub(Vector2 &in) const",
		asMETHODPR(Vector2, operator-, (const Vector2&) const, Vector2), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 opMul(float scalar)",
		asMETHODPR(Vector2, operator*, (float) const, Vector2), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 opDiv(float scalar)",
		asMETHODPR(Vector2, operator/, (float) const, Vector2), asCALL_THISCALL));
	// scalar * vector convenience operator
	check(engine->RegisterObjectMethod("Vector2", "Vector2 opMul_r(float scalar)",
		asFUNCTIONPR(operator*, (float, const Vector2&), Vector2), asCALL_CDECL_OBJLAST));

	// Const Methods
	check(engine->RegisterObjectMethod("Vector2", "float dot(const Vector2 &in) const",
		asMETHOD(Vector2, dot), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "float cross(const Vector2 &in) const",
		asMETHOD(Vector2, cross), asCALL_THISCALL));

	check(engine->RegisterObjectMethod("Vector2", "float get_magnitude() const",
		asMETHOD(Vector2, magnitude), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "float get_angle() const",
		asMETHOD(Vector2, angle), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 get_floor() const",
		asMETHOD(Vector2, floor), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 get_normalized() const",
		asMETHOD(Vector2, normalized), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 projected(Vector2) const",
		asMETHOD(Vector2, projected), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 clamped(const AABB &in) const",
		asMETHOD(Vector2, clamped), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 rotated(float) const",
		asMETHOD(Vector2, rotated), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 get_rotated90CW() const",
		asMETHOD(Vector2, rotated90CW), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "Vector2 get_rotated90CCW() const",
		asMETHOD(Vector2, rotated90CCW), asCALL_THISCALL));

	check(engine->RegisterObjectMethod("Vector2", "void normalize()",
		asMETHOD(Vector2, normalize), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "void round_down()",
		asMETHOD(Vector2, round_down), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "void rotate(float)",
		asMETHOD(Vector2, rotate), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("Vector2", "void clamp(const AABB &in)",
		asMETHOD(Vector2, clamp), asCALL_THISCALL));

	check(engine->RegisterObjectMethod("Vector2", "string to_string() const",
		asFUNCTIONPR(to_string, (const Vector2&), std::string), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterGlobalFunction("string str(const Vector2 &in)",
		asFUNCTIONPR(to_string, (const Vector2&), std::string), asCALL_CDECL));
	check(engine->RegisterGlobalFunction("void println(const Vector2 &in)",
		asFUNCTIONPR(PrintLn, (const Vector2&), void), asCALL_CDECL));

	check(engine->RegisterObjectMethod("Vector2", "float distance_to(const Vector2 &in) const",
		asFUNCTIONPR(distance, (const Vector2&, const Vector2&), float), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterGlobalFunction("float distance (const Vector2 &in, const Vector2 &in)",
		asFUNCTIONPR(distance, (const Vector2&, const Vector2&), float), asCALL_CDECL));
	check(engine->RegisterGlobalFunction("Vector2 Vector2_polar (float angle, float length)",
		asFUNCTION(Vector2::fromPolar), asCALL_CDECL));

	check(engine->RegisterGlobalFunction("Vector2 lerp (Vector2 &in, Vector2 &in, float)",
		asFUNCTIONPR(lerp, (const Vector2&, const Vector2&, float), Vector2), asCALL_CDECL));
	check(engine->RegisterGlobalFunction("Vector2 ease (Vector2 &in, Vector2 &in, float)",
		asFUNCTIONPR(ease, (const Vector2&, const Vector2&, float), Vector2), asCALL_CDECL));

	// === AABB Interface ===

	check(engine->RegisterObjectProperty("AABB", "float left", asOFFSET(AABB, left)));
	check(engine->RegisterObjectProperty("AABB", "float right", asOFFSET(AABB, right)));
	check(engine->RegisterObjectProperty("AABB", "float top", asOFFSET(AABB, top)));
	check(engine->RegisterObjectProperty("AABB", "float bottom", asOFFSET(AABB, bottom)));

	check(engine->RegisterObjectMethod("AABB", "bool contains(const Vector2 &in) const",
		asMETHOD(AABB, contains), asCALL_THISCALL));

	check(engine->RegisterObjectMethod("AABB", "AABB opOr(const AABB &in) const",
		asMETHOD(AABB, operator |), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("AABB", "AABB opAnd(const AABB &in) const",
		asMETHOD(AABB, operator &), asCALL_THISCALL));

	check(engine->RegisterObjectMethod("AABB", "void opOrAssign(const AABB &in)",
		asMETHOD(AABB, operator |=), asCALL_THISCALL));
	check(engine->RegisterObjectMethod("AABB", "void opAndAssign(const AABB &in)",
		asMETHOD(AABB, operator &=), asCALL_THISCALL));

	// === Utility Functions ===

	check(engine->RegisterGlobalFunction("float lerp (float, float, float)",
		asFUNCTIONPR(lerp, (float, float, float), float), asCALL_CDECL));
	check(engine->RegisterGlobalFunction("float ease (float, float, float)",
		asFUNCTIONPR(ease, (float, float, float), float), asCALL_CDECL));
	check(engine->RegisterGlobalFunction("float clamp (float, float, float)",
		asFUNCTION(clamp<float>), asCALL_CDECL));
	check(engine->RegisterGlobalFunction("int clamp (int, int, int)",
		asFUNCTION(clamp<int>), asCALL_CDECL));

	// === Constants ===

	check(engine->SetDefaultNamespace("Vector2"));
	check(engine->RegisterGlobalProperty("const Vector2 ZERO", const_cast<Vector2*>(&Vector2::zero)));
	check(engine->RegisterGlobalProperty("const Vector2 UP", const_cast<Vector2*>(&Vector2::up)));
	check(engine->RegisterGlobalProperty("const Vector2 DOWN", const_cast<Vector2*>(&Vector2::down)));
	check(engine->RegisterGlobalProperty("const Vector2 LEFT", const_cast<Vector2*>(&Vector2::left)));
	check(engine->RegisterGlobalProperty("const Vector2 RIGHT", const_cast<Vector2*>(&Vector2::right)));
	check(engine->SetDefaultNamespace(""));
#undef check
}