
#include "transform.h"
#include <cassert>

const Transform Transform::identity = {
	1.f, 0.f, 0.f,
	0.f, 1.f, 0.f
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
	return {
		t._11 * v.x + t._12 * v.y + t._13,
		t._21 * v.x + t._22 * v.y + t._23
	};
}

Transform Transform::operator ~ () const {
	float det = determinant();

	if (det == 0) { // no inverse
		assert(false && "PANIC: attempt to invert transform with no inverse");
		return {
			NAN, NAN, NAN,
			NAN, NAN, NAN
		};
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