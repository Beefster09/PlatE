#pragma once

#include <cstdint>
#include <cassert>
#include <type_traits>
#include <initializer_list>
#include <memory>
#include <new>
#include "util.h"

// A runtime-fixed-sized array that knows how big it is.
// Essentially a smarter version of a raw pointer.
template <class T>
class Array {
	T* items;
	size_t n_items;

public:
	inline Array() : n_items(0), items(nullptr) {}

	inline Array(T* data, size_t size) {
		n_items = size;
		items = data;
	}

	inline Array(size_t size) {
		n_items = size;
		items = new T[size]();
	}

	Array copy() { // Copies should be explicit.
		unconst<T>::type* buf = new T[n_items];
		if (std::is_trivially_copyable<T>::value) {
			memcpy((void*)buf, (const void*)items, n_items * sizeof(T));
		}
		else {
			for (int i = 0; i < n_items; ++i) {
				new((void*)&buf[i]) T(items[i]);
			}
		}
		return Array(buf, n_items);
	}

	__forceinline Array(const Array& arr) :
		items(arr.items),
		n_items(arr.n_items) {}

	// Convenience constructor
	Array(std::initializer_list<T> ilist) {
		n_items = ilist.size();
		items = new T[n_items];
		int i = 0;
		for (const auto& val : ilist) {
			items[i++] = val;
		}
	}

	void operator = (const Array& arr) {
		items = arr.items;
		n_items = arr.n_items;
	}

	// avoid magical memory management
	__forceinline void free() {
		delete[] items;
		items = nullptr;
	}

	// implicit Array<T> -> Array<const T> conversion
	__forceinline operator Array<const T>() {
		return Array<const T>(items, n_items);
	}

	__forceinline operator const T* () const { return items; }

	__forceinline const T& operator [] (unsigned int index) const {
		assert(index < n_items && "Array bounds check failed");
		return items[index];
	}
	__forceinline const T* begin() const { return items; }
	__forceinline const T* end() const { return items + n_items; }

	__forceinline const T* data() const { return items; }

	__forceinline operator T* () { return items; }

	__forceinline T& operator [] (unsigned int index) {
		assert(index < n_items && "Array bounds check failed");
		return items[index];
	}
	__forceinline T* begin() { return items; }
	__forceinline T* end() { return items + n_items; }

	__forceinline size_t size() const { return n_items; }

	__forceinline T* data() { return items; }

};

template <>
class Array<bool> {
	uint8_t* bytes;
	size_t n_bits;

public:
	__forceinline Array() {}
	Array(uint8_t* arr, size_t size) {
		n_bits = size;
		bytes = arr;
	}
	Array(size_t size) {
		n_bits = size;
		bytes = new uint8_t[(size + 7) / 8];
	}

	void free() {
		delete[] bytes;
		bytes = nullptr;
	}

	__forceinline bool operator [] (unsigned int index) const {
		assert(index < n_bits && "Array bounds check failure");
		return bytes[index / 8] & BIT8(index % 8);
	}
	__forceinline void set(size_t index) { bytes[index / 8] |= BIT8(index % 8); }
	__forceinline void unset(size_t index) { bytes[index / 8] &= ~BIT8(index % 8); }

	__forceinline void clear() { for (int i = 0; i * 8 < n_bits; ++i) bytes[i] = 0; }

	class iterator {
		const Array* arr;
		size_t bit;

	public:
		iterator() { bit = 0; arr = nullptr; }
		iterator(const Array* a, size_t i = 0) { bit = i; arr = a; }
		iterator(iterator& it) { arr = it.arr; bit = it.bit; }
		~iterator() {}

		bool operator == (iterator& it) const { return bit == it.bit && arr == it.arr; }
		__forceinline bool operator != (iterator& it) const { return !(*this == it); }

		__forceinline const bool operator * () const { return (*arr)[bit]; }

		__forceinline iterator& operator ++ () { ++bit; }
	};

	iterator begin() const { return iterator(this); }
	iterator end() const { return iterator(this, n_bits); }

	__forceinline size_t size() const { return n_bits; }
};



template <class T>
class Array2D {
	T* items;
	size_t w, h;

public:
	__forceinline Array2D() {}
	Array2D(T* data, size_t width, size_t height) : w(width), h(height), items(data) {}
	Array2D(size_t width, size_t height) : w(width), h(height),
		items(new T[width * height];) {}

	// avoid magical memory management
	__forceinline void free() {
		delete[] items;
		items = nullptr; w = 0; h = 0;
	}

	__forceinline operator const T* () const { return items; }

	__forceinline const T& operator () (size_t x, size_t y) const {
		assert(x >= 0 && x < w && y >= 0 && y < h && "Array bounds check failed");
		return items[y * w + x];
	}
	__forceinline const T* begin() const { return items; }
	__forceinline const T* end() const { return items + w * h; }

	__forceinline const T* data() const { return items; }

	__forceinline operator T* () { return items; }

	__forceinline T& operator () (size_t x, size_t y) {
		assert(x >= 0 && x < w && y >= 0 && y < h && "Array bounds check failed");
		return items[y * w + x];
	}
	__forceinline T* begin() { return items; }
	__forceinline T* end() { return items + w * h; }

	__forceinline size_t size() const { return w * h; }

	__forceinline size_t width() const { return w; }
	__forceinline size_t height() const { return h; }

	__forceinline T* data() { return items; }
};

template <>
class Array2D<bool> {
	uint8_t* bytes;
	size_t w, h;

public:
	__forceinline Array2D() {}
	Array2D(uint8_t* data, size_t width, size_t height) : w(width), h(height), bytes(data) {}
	Array2D(size_t width, size_t height) : w(width), h(height),
		bytes(new uint8_t[(width * height + 7) / 8]) {}
	Array2D(Array2D& arr) : bytes(arr.bytes), w(arr.w), h(arr.h) {}

	void free() {
		delete[] bytes;
		bytes = nullptr;
	}

	__forceinline bool operator () (size_t x, size_t y) const {
		assert(x >= 0 && x < w && y >= 0 && y < h && "Array bounds check failed");
		int index = y * h + x;
		return bytes[index / 8] & BIT8(index % 8);
	}
	__forceinline void set(size_t x, size_t y) { int index = y * h + x; bytes[index / 8] |= BIT8(index % 8); }
	__forceinline void unset(size_t x, size_t y) { int index = y * h + x; bytes[index / 8] &= ~BIT8(index % 8); }

	__forceinline void clear() { for (int i = 0; i * 8 < w * h; ++i) bytes[i] = 0; }

	class iterator {
		const Array2D* arr;
		size_t bit;

	public:
		iterator() { bit = 0; arr = nullptr; }
		iterator(const Array2D* a, size_t i = 0) { bit = i; arr = a; }
		iterator(iterator& it) { arr = it.arr; bit = it.bit; }
		~iterator() {}

		bool operator == (iterator& it) const { return bit == it.bit && arr == it.arr; }
		__forceinline bool operator != (iterator& it) const { return !(*this == it); }

		__forceinline const bool operator * () const { return (*arr)(bit % arr->w, bit / arr->h); }

		__forceinline iterator& operator ++ () { ++bit; }
	};

	iterator begin() const { return iterator(this); }
	iterator end() const { return iterator(this, w * h); }

	__forceinline size_t size() const { return w * h; }
	__forceinline size_t width() const { return w; }
	__forceinline size_t height() const { return h; }
};