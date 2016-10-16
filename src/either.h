#pragma once

#include <cassert>
#include <type_traits>

// I like either types because they're better for error handling than exceptions
// I also would like to cut the cruft as much as possible here and leave this somewhat unsafe
// and devoid of its typical functional operations.
template<class Left, class Right> struct Either {
	static_assert(!std::is_same<Left, Right>::value, "Either must use different types for Left and Right");
	union { // putting this first allows pointer casting
		Left left;
		Right right;
	};
	bool isLeft;

	Either(Left l) : isLeft(true), left(l) {}
	Either(Right r) : isLeft(false), right(r) {}

	operator Left () const { assert(isLeft && "implicit Either value extraction failed"); return left; }
	operator Right () const { assert(!isLeft && "implicit Either value extraction failed"); return right; }

	Left leftOrElse(Left default) const { return isLeft ? left : default; }
	Right rightOrElse(Right default) const { return isLeft ? default : right; }
};

template <class T>
struct Option {
	static_assert(!std::is_pointer<T>::value, "Option must use a value type. Use a raw pointer instead.");
	static_assert(!std::is_same<T, bool>::value, "Option cannot contain a bool.");
	union {
		T value;
		struct {} _empty_;
	};
	bool isDefined;

	Option(T val) : isDefined(true), value(val) {}
	Option(nullptr_t null) : isDefined(false), _empty_() {}
	Option() : isDefined(false), _empty_() {}

	T orElse(T default) const { return isDefined ? value : default; }

	operator bool() const { return isDefined; }
	operator T() const { assert(isDefined && "implicit Option value extraction failed"); return value; }
};