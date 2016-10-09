#pragma once

#include <cstdint>
#include <cstdio>
#include <type_traits>
#include "either.h"
#include "error.h"
#include "macros.h"

namespace Errors {
	const error_data
		BucketFull = { 40, "Bucket is full and cannot hold any more" },
		BucketIllegalRemove = { 41, "Attempt to remove element from bucket that cannot be removed" };
}

// A runtime-fixed-sized array that knows how big it is. :O
// Why these aren't C++ standard baffles me
template <class Data>
class Array {
	Data* items;
	size_t n_items;

public:
	__forceinline Array() {}
	Array(Data* data, size_t size) : n_items(size), items(data) {}
	Array(size_t size) : n_items(size), items((Data*) operator new(size * sizeof(Data))) {}

	// avoid magical memory management
	__forceinline void free() const {
		delete[] items;
		items = nullptr;
	}

	__forceinline operator const Data* () const { return items; }

	__forceinline const Data& operator [] (unsigned int index) const {
		assert(index < n_items && "Array bounds check failed");
		return items[index];
	}
	__forceinline const Data* begin() const { return items; }
	__forceinline const Data* end() const { return items + n_items; }

	__forceinline operator Data* () { return items; }

	__forceinline Data& operator [] (unsigned int index) {
		assert(index < n_items && "Array bounds check failed");
		return items[index];
	}
	__forceinline Data* begin() { return items; }
	__forceinline Data* end() { return items + n_items; }

	__forceinline size_t size() const { return n_items; }
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
		return bytes[index / 8] & BIT(index % 8);
	}
	__forceinline void set(size_t index) { bytes[index / 8] |= BIT(index % 8); }
	__forceinline void unset(size_t index) { bytes[index / 8] &= ~BIT(index % 8); }

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

		__forceinline iterator& operator ++ () {++bit;}
	};

	iterator begin() const { return iterator(this); }
	iterator end() const { return iterator(this, n_bits); }

	__forceinline size_t size() const { return n_bits; }
};

#define BUCKET_DEFAULT_CAPACITY 256

template <class Data>
class DenseBucket {
private:
	size_t capacity;
	size_t n_items;
	Data* items;

public:
	__forceinline DenseBucket() {}
	DenseBucket(size_t capacity = BUCKET_DEFAULT_CAPACITY) {
		this->capacity = capacity;
		n_items = 0;
		items = new Data[capacity];
	}

	__forceinline void free() {
		delete[] items;
	}

	Either<Error, Data*> add(Data& data) {
		if (n_items >= capacity) {
			return Errors::BucketFull;
		}
		items[n_items] = data;
		Data* result = items + n_items;
		++n_items;
		return result;
	}

	Option<Error> remove(Data* item) {
		auto offset = item - items;
		if (offset < 0 || offset >= n_items) {
			return Errors::BucketIllegalRemove;
		}
		items[offset] = items[--n_items];
		return Option<Error>::None;
	}

	__forceinline Data* begin() { return items; }
	__forceinline Data* end() { return items + n_items; }

	__forceinline size_t size() const { return n_items; }
	Data* at(size_t index) { return (index >= 0 && index < n_items) ? items + index : nullptr; }
	Data* operator [] (size_t index) { return items + index; }
};

template <class Data>
class SparseBucket {
private:
	size_t capacity;
	size_t n_items;
	Array<bool> occupation;
	Data* items;
	size_t next_index;

public:
	__forceinline SparseBucket() {}
	SparseBucket(size_t capacity) {
		this->capacity = capacity;
		n_items = 0;
		// minimize allocations
		int occ_mask_len = (capacity + 7) / 8;
		void* pool = operator new(capacity * sizeof(Data) + occ_mask_len);
		items = (Data*) pool;
		occupation = Array<bool>((uint8_t*) pool + capacity * sizeof(Data), capacity);
		occupation.clear();

		next_index = 0;
	}

	void free() {
		delete[] (void*) items;
	}

	void print_contents() {
		printf("Occupation:\n");
		for (bool bit : occupation) {
			printf("%c", bit ? '1' : '0');
		}
		printf("\n");
	}

	Either<Error, Data*> add(Data& data) {
		if (n_items >= capacity) {
			return Errors::BucketFull;
		}

		items[next_index] = data;
		// Set the bit in the occupy mask
		occupation.set(next_index);

		Data* result = items + next_index;

		++n_items;
		if (n_items >= capacity) return result;

		// Find next available index
		do {
			next_index = (next_index + 1) % capacity;
		} while (occupation[next_index]);

		return result;
	}

	Option<Error> remove(Data* item) {
		auto offset = item - items;
		if (offset < 0 || offset >= n_items || !occupation[offset]) {
			return Errors::BucketIllegalRemove;
		}
		// Unset the bit :O
		occupation.unset(offset);

		// Avoid searching next time if this bucket was *just* full.
		if (n_items >= capacity) {
			next_index = offset;
		}

		--n_items;
		return nullptr;
	}

	class iterator {
		SparseBucket* b;
		size_t index;
	public:
		iterator() { index = 0; b = nullptr; }
		iterator(SparseBucket* bucket) {
			b = bucket;
			index = 0;
			while (!b->occupation[index]) {
				++index;
			}
		}
		iterator(SparseBucket* bucket, size_t i) { index = i; b = bucket; }
		iterator(iterator& it) { index = it.index; b = it.b; }
		~iterator() {}

		bool operator == (iterator& it) const { return index == it.index && b == it.b; }
		__forceinline bool operator != (iterator& it) const { return !(*this == it); }

		__forceinline const Data& operator * () const { return b->items[index]; }
		__forceinline Data& operator * () { return b->items[index]; }

		iterator& operator ++ () {
			do {
				++index;
				if (index >= b->capacity) break;
			} while (!b->occupation[index]);
			return *this;
		}
	};

	__forceinline iterator begin() { return iterator(this); }
	__forceinline iterator end() { return iterator(this, capacity); }

	__forceinline size_t size() const { return n_items; }
};

template <class Data>
class Array2D {
	Data* items;
	size_t w, h;

public:
	__forceinline Array2D() {}
	Array2D(Data* data, size_t width, size_t height) : w(width), h(height), items(data) {}
	Array2D(size_t width, size_t height) : w(width), h(height),
		items(new Data[width * height];) {}

	// avoid magical memory management
	__forceinline void free() {
		delete[] items;
		items = nullptr; w = 0; h = 0;
	}

	struct tile { unsigned int x, y; };

	__forceinline operator const Data* () const { return items; }

	__forceinline const Data& operator [] (tile index) const {
		assert (index.x >= 0 && index.x < w && index.y >= 0 && index.y < h && "Array bounds check failed")
		return items[index.y * w + index.x];
	}
	__forceinline const Data* begin() const { return items; }
	__forceinline const Data* end() const { return items + w * h; }

	__forceinline operator Data* () { return items; }

	__forceinline Data& operator [] (tile index) {
		assert(index.x >= 0 && index.x < w && index.y >= 0 && index.y < h && "Array bounds check failed")
		return items[index.y * w + index.x];
	}
	__forceinline Data* begin() { return items; }
	__forceinline Data* end() { return items + w * h; }

	__forceinline size_t size() const { return w * h; }

	__forceinline size_t width() const { return w; }
	__forceinline size_t height() const { return h; }
};

// If a memory pool is owned, it will free the memory when it goes out of scope. Use with care.
template <bool owned = false>
class MemoryPool_raw {
	void* pool;
	void* nextAlloc;
	size_t size;

public:
	__forceinline MemoryPool_raw() {}
	MemoryPool_raw(void* pool, size_t poolsize) {
		this->pool = pool;
		nextAlloc = pool;
		size = poolsize;
	}
	MemoryPool_raw(size_t poolsize) {
		pool = new uint8_t[poolsize];
		nextAlloc = pool;
		size = poolsize;
	}
	~MemoryPool_raw() {
		if (owned) this->free(); // this code will be eliminated in the unowned version.
	}

	template <class T>
	T* alloc(size_t n_elements = 1) {
		if ( (intptr_t)nextAlloc - (intptr_t)pool + n_elements * sizeof(T) > size) return nullptr;
		T* result = (T*) nextAlloc;
		nextAlloc = result + n_elements;
		return result;
	}

	char* alloc_str(const char* str) {
		size_t str_size = strlen(str) + 1; // +1 for the null terminator
		if ((intptr_t)nextAlloc - (intptr_t)pool + str_size > size) return nullptr;
		char* result = (char*)nextAlloc;
		strcpy(result, str);
		nextAlloc = result + str_size;
		return result;
	}

	/** 'Free' everything allocated in this pool
	* Resets allocation pointer to the beginning of the pool.
	* All memory is free to be overwritten. Destructors will not be run.
	* Use with care.
	*/
	__forceinline void clear() { nextAlloc = pool; }

	/** Free everything allocated in this pool
	* Destructors will not be run.
	* Use with care.
	*/
	__forceinline void free() { operator delete(pool); }
};

typedef MemoryPool_raw<false> MemoryPool;
typedef MemoryPool_raw<true> OwnedMemoryPool;