
#include "transform.h"
#include "hitbox.h"
#include <cassert>

const Transform Transform::identity = {
	1.f, 0.f, 0.f,
	0.f, 1.f, 0.f
};

const Transform Transform::invalid = {
	NAN, NAN, NAN,
	NAN, NAN, NAN
};

const float Transform::_31 = 0.f;
const float Transform::_32 = 0.f;
const float Transform::_33 = 1.f;

Transform operator * (const Transform& lhs, const Transform& rhs) {
	return {
		lhs._11 * rhs._11 + lhs._12 * rhs._21,
		lhs._11 * rhs._12 + lhs._12 * rhs._22,
		lhs._11 * rhs._13 + lhs._12 * rhs._23 + lhs._13,
		lhs._21 * rhs._11 + lhs._22 * rhs._21,
		lhs._21 * rhs._12 + lhs._22 * rhs._22,
		lhs._21 * rhs._13 + lhs._22 * rhs._23 + lhs._23
	};
}

Vector2 operator * (const Transform& t, const Vector2& v) {
	return{
		t._11 * v.x + t._12 * v.y + t._13,
		t._21 * v.x + t._22 * v.y + t._23
	};
}

Line operator * (const Transform& tx, const Line& l) {
	return{ tx * l.p1, tx * l.p2 };
}

Circle operator * (const Transform& tx, const Circle& circ) {
	return{
		{circ.center.x + tx._13, circ.center.y + tx._23},
		fmaxf(tx.get_scaleX(), tx.get_scaleY()) * circ.radius
	};
}

AABB operator * (const Transform& t, const AABB& box) {
	if (t.is_identity()) {
		return box;
	}
	else if (t.is_translate_only()) {
		return{
			box.left + t._13,
			box.right + t._13,
			box.top + t._23,
			box.bottom + t._23
		};
	}
	else if (t.is_rect_invariant()) {
		Vector2 tl = t * Vector2{ box.left, box.top };
		Vector2 br = t * Vector2{ box.right, box.bottom };
		return{
			fminf(tl.x, br.x),
			fmaxf(tl.x, br.x),
			fminf(tl.y, br.y),
			fmaxf(tl.y, br.y)
		};
	}
	else {
		Vector2 tl = t * Vector2{ box.left, box.top };
		Vector2 tr = t * Vector2{ box.right, box.top };
		Vector2 bl = t * Vector2{ box.left, box.bottom };
		Vector2 br = t * Vector2{ box.right, box.bottom };
		return{
			fminf(fminf(tl.x, tr.x), fminf(bl.x, br.x)),
			fmaxf(fmaxf(tl.x, tr.x), fmaxf(bl.x, br.x)),
			fminf(fminf(tl.y, tr.y), fminf(bl.y, br.y)),
			fmaxf(fmaxf(tl.y, tr.y), fmaxf(bl.y, br.y))
		};
	}
}

void Transform::transform_all(Array<Vector2> vecs) const {
	for (Vector2& vec : vecs) {
		vec = *this * vec;
	}
}

Transform Transform::operator ~ () const {
	float det = determinant();

	if (det == 0) { // no inverse
		assert(false && "PANIC: attempt to invert transform with no inverse");
		return invalid;
	}

	return {
		_22 / det, -_12 / det, (_12 * _23 - _13 * _22) / det,
		-_21 / det, _11 / det, (_13 * _21 - _11 * _23) / det
		//   0          0      ((_11 * _22 - _21 * _12) ==> det) / det ==> 1
	};
}

float Transform::determinant() const {
	return _11 * _22 - _21 * _12; // because the bottom row is [ 0  0  1 ]
}

float Transform::get_scaleX() const {
	return copysignf(sqrt(_11 * _11 + _12 * _12), _11);
}

float Transform::get_scaleY() const {
	return copysignf(sqrt(_21 * _21 + _22 * _22), _22);
}

float Transform::get_rotation() const {
	return atan2(_22, _21);
}

bool Transform::is_uniform_scale() const {
	float sx2 = _11 * _11 + _12 * _12;
	float sy2 = _21 * _21 + _22 * _22;
	return FLOAT_EQ(sx2, sy2);
}