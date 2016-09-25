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

#define BUCKET_DEFAULT_CAPACITY 256

template <class Data>
class DenseBucket {
private:
	size_t capacity;
	size_t n_items;
	Data* items;

public:
	DenseBucket(size_t capacity = BUCKET_DEFAULT_CAPACITY) {
		this->capacity = capacity;
		n_items = 0;
		items = new Data[capacity];
	}

	~DenseBucket() {
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
	Data* items;
	uint8_t* occupy_mask;
	size_t next_index;

public:
	SparseBucket(size_t capacity = BUCKET_DEFAULT_CAPACITY) {
		this->capacity = capacity;
		n_items = 0;
		// minimize allocations
		int occ_mask_len = (capacity + 7) / 8;
		void* pool = operator new(capacity * sizeof(Data) + occ_mask_len);
		items = (Data*) pool;
		occupy_mask = (uint8_t*) pool + capacity * sizeof(Data);

		// set bucket to empty
		for (int i = 0; i < occ_mask_len; ++i) {
			occupy_mask[i] = 0;
		}

		next_index = 0;
	}
	~SparseBucket() {
		delete[] (void*) items;
	}

	void print_contents() {
		printf("Occupation:\n");
		for (int i = 0; i < capacity; ++i) {
			printf("%c", (occupy_mask[i / 8] & BIT(i % 8))? '1' : '0');
		}
		printf("\n");
	}

	Either<Error, Data*> add(Data& data) {
		if (n_items >= capacity) {
			return Errors::BucketFull;
		}

		items[next_index] = data;
		// Set the bit in the occupy mask
		occupy_mask[next_index / 8] |= BIT(next_index % 8);

		Data* result = items + next_index;

		++n_items;
		if (n_items >= capacity) return result;

		// Find next available index
		do {
			next_index = (next_index + 1) % capacity;
		} while (occupy_mask[next_index / 8] & BIT(next_index % 8));

		return result;
	}

	Option<Error> remove(Data* item) {
		auto offset = item - items;
		if (offset < 0 || offset >= n_items || (occupy_mask[offset / 8] & (1 << (offset % 8))) == 0) {
			return Errors::BucketIllegalRemove;
		}
		// Unset the bit :O
		occupy_mask[offset / 8] &= ~((uint8_t)BIT(offset % 8));

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
			while (!(b->occupy_mask[index / 8] & BIT(index % 8))) {
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
			} while (!(b->occupy_mask[index / 8] & BIT(index % 8)));
			return *this;
		}
	};

	__forceinline iterator begin() { return iterator(this); }
	__forceinline iterator end() { return iterator(this, capacity); }

	__forceinline size_t size() const { return n_items; }
};

/*
template<class Data>
class BucketAllocator {

	Data* alloc();
	void free(Data*);
};
*/

// A runtime-fixed-sized array that knows how big it is. :O
// Why these aren't C++ standard baffles me
template <class Data>
class Array {
	Data* items;
	size_t n_items;

public:
	Array(Data* data, size_t size) : n_items(size), items(data) {}
	Array(size_t size) : n_items(size), items((Data*) operator new(size * sizeof(Data))) {}

	// avoid magical memory management
	__forceinline void free() const { delete[] items; }

	__forceinline operator const Data* () const { return items; }

	__forceinline const Data& operator [] (size_t index) const { return items[index]; }
	__forceinline const Data* begin() const { return items; }
	__forceinline const Data* end() const { return items + n_items; }

	__forceinline operator Data* () { return items; }

	__forceinline Data& operator [] (size_t index) { return items[index]; }
	__forceinline Data* begin() { return items; }
	__forceinline Data* end() { return items + n_items; }

	__forceinline size_t size() const { return n_items; }
};

class MemoryPool {
	void* pool;
	void* nextAlloc;
	size_t size;

public:
	MemoryPool(size_t poolsize);

	template <class T>
	T* alloc(size_t n_elements = 0) {
		if ( (intptr_t)nextAlloc - (intptr_t)pool + n_elements * sizeof(T) > size) return nullptr;
		T* result = (T*) nextAlloc;
		nextAlloc = result + n_elements;
		return result;
	}

	char* alloc_str(const char* str);
};