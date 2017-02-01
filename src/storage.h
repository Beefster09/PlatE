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

namespace Errors {
	const error_data
		BucketFull = { 40, "Bucket is full and cannot hold any more" },
		BucketIllegalRemove = { 41, "Attempt to remove element from bucket that cannot be removed" };
}

template <typename T>
struct unconst {
	typedef T type;
};

template <typename T>
struct unconst<const T> {
	typedef T type;
};

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
			memcpy((void*)buf, (const void*) items, n_items * sizeof(T));
		}
		else {
			for (int i = 0; i < n_items; ++i) {
				new((void*) &buf[i]) T(items[i]);
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

// Possible alternative implementation
//// Trivially copyable/movable/assignable array of variable length
//template <class T>
//class ArrayPtr {
//private:
//	struct __arr__ {
//		size_t len;
//		T first;
//	} *data;
//
//	inline ArrayPtr(__arr__* ptr) { data = ptr; }
//public:
//	inline ArrayPtr() : data(nullptr) {}
//	inline ArrayPtr(size_t len) {
//		data = operator new(sizeof(size_t) + len * sizeof(T));
//		data->len = len;
//	}
//
//	ArrayPtr copy() {
//		ArrayPtr arrcpy(data->len);
//		if (std::is_trivially_copyable<T>::value) {
//			memcpy(&arrcpy.data->first, &data->first, data->len);
//		}
//		else {
//			for (T* iptr = begin(), iend = end(); iptr < iend; ++iptr) {
//				new(iptr) T(&iptr);
//			}
//		}
//	}
//
//	inline void free() {
//		if (data == nullptr) return;
//		operator delete(data);
//		data = nullptr;
//	}
//
//	// conversion from ArrayPtr<T> -> ArrayPtr<const T>
//	inline operator ArrayPtr<const T> () {
//		return ArrayPtr<const T>(reinterpret_cast<ArrayPtr<const T>::__arr__*>(data));
//	}
//
//	inline T& operator [] (size_t index) {
//		assert(index >= 0 && index < data->len);
//		return (&data->first)[index];
//	}
//
//	inline const T& operator [] (size_t index) const {
//		assert(index >= 0 && index < data->len);
//		return (&data->first)[index];
//	}
//
//	inline operator T* () {
//		return &data->first;
//	}
//
//	inline size_t size() const {
//		return data->len;
//	}
//
//	inline T* begin() { return &data->first; }
//	inline T* end() { return (&data->first)[data->len]; }
//
//	inline const T* begin() const { return &data->first; }
//	inline const T* end() const { return (&data->first)[data->len]; }
//
//	inline T* c_ptr() { return &data->first; }
//};

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
	//Array(Array& arr) = default;
	//Array& operator = (Array& arr) = default;

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

		__forceinline iterator& operator ++ () {++bit;}
	};

	iterator begin() const { return iterator(this); }
	iterator end() const { return iterator(this, n_bits); }

	__forceinline size_t size() const { return n_bits; }
};

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

// The size increments that memory pools should stay in
#define MEMORYPOOL_GRANULARITY 512

// If a memory pool is owned, it will free the memory when it goes out of scope. Use with care.
template <bool owned = false>
class MemoryPool_raw {
	void* pool;
	void* nextAlloc;
	size_t size;
	size_t free_space;

public:
	__forceinline MemoryPool_raw() {}
	MemoryPool_raw(void* pool, size_t poolsize) {
		this->pool = pool;
		nextAlloc = pool;
		size = poolsize;
		free_space = size;
	}
	MemoryPool_raw(size_t poolsize) {
		size = poolsize + (512 - poolsize % MEMORYPOOL_GRANULARITY);
		pool = operator new (size);
		nextAlloc = pool;
		free_space = size;
	}
	MemoryPool_raw(MemoryPool_raw& other) = delete;
	~MemoryPool_raw() {
		if (owned) this->free(); // this code will be eliminated in the unowned version.
	}

	template <class T>
	T* alloc(int n_elements = 1) {
		if (n_elements <= 0) return nullptr;
		T* result = reinterpret_cast<T*>(std::align(alignof(T), sizeof(T) * n_elements, nextAlloc, free_space));
		if (result == nullptr) {
			return nullptr;
		}
		nextAlloc = result + n_elements;
		free_space -= sizeof(T) * n_elements;
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
	__forceinline void clear() {
		nextAlloc = pool;
		free_space = size;
	}

	/** Free everything allocated in this pool
	* Destructors will not be run.
	* Use with care.
	*/
	__forceinline void free() {
		operator delete(pool);
	}

	__forceinline size_t get_slack() {
		return free_space;
	}

	__forceinline size_t get_size() {
		return size;
	}
};

typedef MemoryPool_raw<false> MemoryPool;
typedef MemoryPool_raw<true> OwnedMemoryPool;

template <size_t size>
/// Memory pool that is stored on the stack
class LocalMemoryPool {
	uint8_t pool[size];
	void* nextAlloc;

public:
	LocalMemoryPool() {
		nextAlloc = pool;
	}
	~LocalMemoryPool() {}

	template <class T>
	T* alloc(size_t n_elements = 1) {
		if ((intptr_t)nextAlloc - (intptr_t)pool + n_elements * sizeof(T) > size) return nullptr;
		T* result = (T*) std::align(nextAlloc);
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

	__forceinline void clear() { nextAlloc = pool; }
};

template<class T, int MaxEmptyBuckets = 1, int NewBucketSize = 64, int BucketGroupInitialSize = 16>
class BucketAllocator{
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