#pragma once

#include <cmath>
#include "vectors.h"

/// 3x3 Transformation matrix that assumes homogeneous column vectors and a [0 0 1] bottom row.
// This will be vital to collision detection and rendering logic.
struct Transform {
	float _11, _12, _13;
	float _21, _22, _23;
	//     0    0    1

	static const Transform identity;

	// For completeness' sake:
	static const float _31, _32, _33;

	// basic rotation matrix
	static inline Transform rotation(float angle) {
		float s = sinf(angle);
		float c = cosf(angle);
		return {
			c, -s, 0.f,
			s, c, 0.f
		};
	}

	// basic translation matrix
	static inline Transform translation(Vector2 vec) {
		return {
			1.f, 0.f, vec.x,
			0.f, 1.f, vec.y
		};
	}

	// basic scale matrix
	static inline Transform scaling(Vector2 vec) {
		return {
			vec.x, 0.f, 0.f,
			0.f, vec.y, 0.f
		};
	}

	// basic uniform scale matrix
	static __forceinline Transform scaling(float xy) { return scaling(Vector2{ xy, xy }); }

	// rotation then translation
	static inline Transform rot_trans(float angle, Vector2 pos) {
		float s = sinf(angle);
		float c = cosf(angle);
		return{
			c, -s, pos.x,
			s, c, pos.y
		};
	}

	// scaling then translation
	static inline Transform scal_trans(Vector2 scal, Vector2 pos) {
		return{
			scal.x, 0.f, pos.x,
			0.f, scal.y, pos.y
		};
	}

	// scaling then rotation then translation
	static inline Transform scal_rot_trans(Vector2 scal, float angle, Vector2 pos) {
		float s = sinf(angle);
		float c = cosf(angle);
		return{
			c * scal.x, -s * scal.y, pos.x,
			s * scal.x, c * scal.y, pos.y
		};
	}

	// uniform scaling then rotation then translation
	static inline Transform scal_rot_trans(float scaleXY, float angle, Vector2 pos) {
		float s = sinf(angle) * scaleXY;
		float c = cosf(angle) * scaleXY;
		return{
			c, -s, pos.x,
			s, c, pos.y
		};
	}

	// transform matrix multiplication
	friend Transform operator * (const Transform& lhs, const Transform& rhs);

	// apply translation to vector
	friend Vector2 operator * (const Transform& transform, const Vector2& vector);

	// inverted matrix
	Transform operator ~ () const;

	// determinant of this matrix
	float determinant() const;

	// modify this matrix by doing the given transform after the current one. Chainable.
	__forceinline Transform& apply(Transform& next) {
		return *this = next * *this;
	}

	// apply a scale factor to the current transform. Chainable.
	inline Transform& scale(Vector2 scal) {
		return apply(Transform::scaling(scal));
	}

	// apply a uniform scale factor to the current transform. Chainable.
	inline Transform& scale(float xy) {
		return apply(Transform::scaling(xy));
	}

	// apply a translation to the current transform. Chainable.
	inline Transform& translate(float x, float y) {
		_13 += x;
		_23 += y;
		return *this;
	}

	// apply a rotation to the current transform. Chainable.
	inline Transform& rotate(float angle) {
		return apply(Transform::rotation(angle));
	}

	// give a transform that does this transform and then the given one
	__forceinline Transform compose(Transform& next) const {
		return next * *this;
	}

	// give a transform that does this transform and then scales it.
	inline Transform scaled(Vector2 scal) const {
		return compose(Transform::scaling(scal));
	}

	// give a transform that does this transform and then uniformly scales it.
	inline Transform scaled(float xy) const {
		return compose(Transform::scaling(xy));
	}

	// give a transform that does this transform and then translates it.
	inline Transform translated(float x, float y) const {
		return {
			_11, _12, x,
			_21, _22, y
		};
	}

	// give a transform that does this transform and then rotates it.
	inline Transform rotated(float angle) const {
		return compose(Transform::rotation(angle));
	}
};