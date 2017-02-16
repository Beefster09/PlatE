#pragma once

#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <initializer_list>
#include <memory>
#include <new>
#include "result.h"
#include "error.h"
#include "util.h"
//#include "arrays.h"

namespace Errors {
	const error_data
		BucketFull = { 40, "Bucket is full and cannot hold any more" },
		BucketIllegalRemove = { 41, "Attempt to remove element from bucket that cannot be removed" };
}

template <typename T, bool autoexpand = false>
class DenseBucket {
	static_assert(std::is_trivially_copyable<T>::value, "T is not trivially copyable");
	static_assert(std::is_move_assignable<T>::value, "T is not move assignable");
private:
	size_t capacity_;
	size_t n_items;
	T* items;

public:
	__forceinline DenseBucket() {}
	DenseBucket(size_t capacity_) {
		this->capacity_ = capacity_;
		n_items = 0;
		items = new T[capacity_];
	}

	__forceinline void free() {
		delete[] items;
	}

	T* add(T& data) {
		if (n_items >= capacity_) {
			if (autoexpand) {
				reserve(capacity_ << 1);
			}
			else {
				return nullptr;
			}
		}
		items[n_items] = data;
		T* result = items + n_items;
		++n_items;
		return result;
	}

	bool remove(size_t index) {
		if (index < 0 || index >= n_items) {
			return false;
		}
		if (!std::is_pod<T>::value) {
			items[index].T::~T();
		}
		items[index] = items[--n_items];
		return true;
	}

	void reserve(size_t new_capacity) {
		T* new_items = new T[new_capacity];
		memcpy(new_items, items, n_items * sizeof(T));
		delete[] items;
		items = new_items;
	}

	__forceinline T* begin() { return items; }
	__forceinline T* end() { return items + n_items; }

	__forceinline size_t size() const { return n_items; }
	__forceinline size_t capacity() const { return capacity_; }

	T& operator [] (size_t index) { return items[index]; }
};

template <class T>
class SparseBucket {
private:
	size_t capacity_;
	size_t n_items;
	Array<bool> occupation;
	T* items;
	size_t next_index;

public:
	__forceinline SparseBucket() {}
	SparseBucket(size_t capacity) {
		capacity_ = capacity;
		n_items = 0;
		// minimize allocations
		int occ_mask_len = (capacity + 7) / 8;
		void* pool = operator new(capacity * sizeof(T) + occ_mask_len);
		items = (T*) pool;
		occupation = Array<bool>((uint8_t*) pool + capacity * sizeof(T), capacity);
		occupation.clear();

		next_index = 0;
	}

	~SparseBucket() {
		operator delete(items);
	}

	void print_contents() {
		printf("Occupation:\n");
		for (bool bit : occupation) {
			printf("%c", bit ? '1' : '0');
		}
		printf("\n");
	}

	/// reserve a spot but don't set the data
	Result<T*> add() {
		if (n_items >= capacity_) {
			return Errors::BucketFull;
		}

		// Set the bit in the occupy mask
		occupation.set(next_index);

		T* result = items + next_index;

		++n_items;
		if (n_items >= capacity_) return result;

		// Find next available index
		do {
			next_index = (next_index + 1) % capacity_;
		} while (occupation[next_index]);

		return result;
	}

	Result<T*> add(const T& data) {
		if (n_items >= capacity_) {
			return Errors::BucketFull;
		}

		items[next_index] = data;
		// Set the bit in the occupy mask
		occupation.set(next_index);

		T* result = items + next_index;

		++n_items;
		if (n_items >= capacity_) return result;

		// Find next available index
		do {
			next_index = (next_index + 1) % capacity_;
		} while (occupation[next_index]);

		return result;
	}

	Result<> remove(int offset) {
		if (offset < 0 || offset >= n_items || !occupation[offset]) {
			return Errors::BucketIllegalRemove;
		}
		// Unset the bit :O
		occupation.unset(offset);
		items[offset].~Data();

		// Avoid searching next time if this bucket was *just* full.
		if (n_items >= capacity_) {
			next_index = offset;
		}

		--n_items;
		return nullptr;
	}

	Result<> remove(T* ptr) {
		int offset = static_cast<int>(ptr - items) / sizeof(T);
		if (offset < 0 || offset >= n_items || !occupation[offset]) {
			return Errors::BucketIllegalRemove;
		}
		// Unset the bit :O
		occupation.unset(offset);
		items[offset].~T();

		// Avoid searching next time if this bucket was *just* full.
		if (n_items >= capacity_) {
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

		__forceinline const T& operator * () const { return b->items[index]; }
		__forceinline T& operator * () { return b->items[index]; }

		iterator& operator ++ () {
			do {
				++index;
				if (index >= b->capacity_) break;
			} while (!b->occupation[index]);
			return *this;
		}
	};

	__forceinline iterator begin() { return iterator(this); }
	__forceinline iterator end() { return iterator(this, capacity_); }

	__forceinline size_t size() const { return n_items; }
	__forceinline size_t capacity() const { return capacity_; }

	inline bool contains_ptr(T* ptr) const {
		return ptr >= items && ptr < items + n_items;
	}
};

template<class T, int MaxEmptyBuckets = 1, int NewBucketSize = 64, int BucketGroupInitialSize = 16>
class BucketAllocator {
public:
	typedef SparseBucket<T> Bucket;
	typedef DenseBucket<Bucket*, true> BucketGroup;
private:
	BucketGroup unfull_buckets, full_buckets;
	int empty_count = 0;
public:
	BucketAllocator() : full_buckets(BucketGroupInitialSize), unfull_buckets(BucketGroupInitialSize) {}
	BucketAllocator(BucketAllocator&&) = delete;
	BucketAllocator(const BucketAllocator&) = delete;

	~BucketAllocator() {
		for (Bucket* bucket : full_buckets) {
			for (T& item : *bucket) {
				item.~T();
			}
			delete bucket;
		}
		for (Bucket* bucket : unfull_buckets) {
			for (T& item : *bucket) {
				item.~T();
			}
			delete bucket;
		}
	}

	T* alloc() {
		if (unfull_buckets.size() == 0) {
			Bucket* bucket = new Bucket(NewBucketSize);
			unfull_buckets.add(bucket);
			++empty_count;
		}
		Bucket* bucket = unfull_buckets[0];
		if (bucket->size() == 0) { // the empty bucket is no longer empty
			--empty_count;
		}
		T* ptr = bucket->add();
		if (bucket->size() >= bucket->capacity()) {
			full_buckets.add(bucket);
			unfull_buckets.remove(0);
		}
		return ptr;
	}

	bool free(T* ptr) {
		Bucket* owning_bucket = nullptr;
		int ob_ind = -1;
		// check full buckets first, since they are more likely to contain the pointer.
		size_t n_buckets = full_buckets.size();
		for (int i = 0; i < n_buckets; ++i) {
			Bucket* bucket = full_buckets[i];
			if (bucket->contains_ptr(ptr)) {
				owning_bucket = bucket;
				ob_ind = i;
				break;
			}
		}
		if (owning_bucket == nullptr) {
			size_t n_buckets = unfull_buckets.size();
			for (int i = 0; i < n_buckets; ++i) {
				Bucket* bucket = unfull_buckets[i];
				if (bucket->contains_ptr(ptr)) {
					owning_bucket = bucket;
					ob_ind = i;
					break;
				}
			}
			if (owning_bucket == nullptr) {
				return false; // pointer not owned; unable to deallocate
			}

			// found in unfull bucket
			owning_bucket->remove(ptr);

			if (owning_bucket->size() == 0) {
				if (++empty_count > MaxEmptyBuckets) { // remove bucket if there is already another empty one
					unfull_buckets.remove(ob_ind);
					delete owning_bucket;
					--empty_count;
				}
			}
		}
		else {
			owning_bucket->remove(ptr);

			// bucket has space now, so move it to the unfull group
			unfull_buckets.add(owning_bucket);
			full_buckets.remove(ob_ind);
		}
		return true;
	}
};