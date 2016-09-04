#pragma once

#include <cassert>
#include <type_traits>

// I like either types because they're better for error handling than exceptions
// I also would like to cut the cruft as much as possible here and leave this somewhat unsafe
// and devoid of its typical functional operations.
// It retains its typical immutability though.
template<class Left, class Right> class Either {
	static_assert(!std::is_same<Left, Right>::value, "Either must use different types for Left and Right");
public:
	const union { // putting this first allows pointer casting
		Left left;
		Right right;
	};
	const bool isLeft;

	// I needed it to be a class so I could have implicit conversion from its constituent types
	Either(Left l) : isLeft(true), left(l) {}
	Either(Right r) : isLeft(false), right(r) {}
};

template<class Left, class Right> bool operator == (const Either<Left, Right>& lhs, const Either<Left, Right>& rhs) {
	if (rhs.isLeft != lhs.isLeft) return false;
	else if (rhs.isLeft) return lhs.left == rhs.left;
	else return lhs.right == rhs.right;
}

template<class A, class B> bool operator == (const Either<A, B>& lhs, const Either<B, A>& rhs) {
	if (rhs.isLeft == lhs.isLeft) return false;
	else if (lhs.isLeft) return lhs.left == rhs.right;
	else return lhs.right == rhs.left;
}

template<class Left, class Right> bool operator == (const Either<Left, Right>& lhs, const Left& rhs) {
	if (!rhs.isLeft) return false;
	else return lhs.left = rhs;
}

template<class Left, class Right> bool operator == (const Either<Left, Right>& lhs, const Right& rhs) {
	if (rhs.isLeft) return false;
	else return lhs.right = rhs;
}

template<class Left, class Right> inline bool operator == (const Left& lhs, const Either<Left, Right>& rhs) {
	return rhs == lhs;
}

template<class Left, class Right> inline bool operator == (const Right& lhs, const Either<Left, Right>& rhs) {
	return rhs == lhs;
}

template<class Left, class Right> inline bool operator != (const Either<Left, Right>& lhs, const Either<Left, Right>& rhs) {
	return !(lhs == rhs);
}

template<class A, class B> inline bool operator != (const Either<A, B>& lhs, const Either<B, A>& rhs) {
	return !(lhs == rhs);
}

template<class Left, class Right> inline bool operator != (const Either<Left, Right>& lhs, const Left& rhs) {
	return !(lhs == rhs);
}

template<class Left, class Right> inline bool operator != (const Either<Left, Right>& lhs, const Right& rhs) {
	return !(lhs == rhs);
}

template<class Left, class Right> inline bool operator != (const Left& lhs, const Either<Left, Right>& rhs) {
	return !(rhs == lhs);
}

template<class Left, class Right> inline bool operator != (const Right& lhs, const Either<Left, Right>& rhs) {
	return !(rhs == lhs);
}