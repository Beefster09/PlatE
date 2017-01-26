#pragma once

#include <cassert>
#include <type_traits>
#include "error.h"

/// Value that represents a value if successful or an error if not
template <class T = void>
struct Result {
	static_assert(!std::is_same<T, Error>::value, "Result<T> cannot have Error as its type");
	union {
		T value;
		Error err;
	};
	bool isSuccess;

	inline Result(const T& val) : isSuccess(true), value(val) {}
	inline Result(const Error& err) : isSuccess(false), err(err) {}
	inline Result(const Errors::error_data& err) : isSuccess(false), err(Error(err)) {}

	Result(const Result& other) {
		isSuccess = other.isSuccess;
		if (isSuccess) {
			value = other.value;
		}
		else {
			err = other.err;
		}
	}

	Result(Result&& other) 
	{
		isSuccess = std::move(other.isSuccess);
		if (other.isSuccess) { // Hack to avoid triggering destructors on garbage
			new(&value) T(std::move(other.value));
		}
		else { // Hack to avoid triggering destructors on garbage
			new(&err) Error(std::move(other.err));
		}
	}

	~Result() {
		if (isSuccess) {
			value.~T();
		}
		else {
			err.~Error();
		}
	}

	inline T orElse(T default) const {
		return isDefined ? value : default;
	}

	inline operator bool() const { return isSuccess; }

	// Implicit extraction (disabled if T == bool)
	template <typename Q = T>
	inline operator typename std::enable_if<!std::is_same<Q, bool>::value, T>::type() const {
		assert(isSuccess && "implicit Result value extraction failed");
		return value;
	}
};

template <>
struct Result<void> {
	Error err;
	bool isSuccess;

	inline Result(const Error& err) : isSuccess(false), err(err) {}
	inline Result(const Errors::error_data& err) : isSuccess(false), err(Error(err)) {}
	inline Result(nullptr_t null) : isSuccess(true), err() {}

	inline operator bool() const { return isSuccess; }

	static const Result<void> success;
};