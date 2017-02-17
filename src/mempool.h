#pragma once

#include <cstdint>
#include <cassert>
#include <type_traits>
#include <memory>
#include <new>
#include <atomic>
#include "util.h"

// The size increments that memory pools should stay in
constexpr size_t MEMORYPOOL_GRANULARITY = 512;
constexpr size_t ALIGNMENT = alignof(intmax_t);

class MemoryPool {
	void* const pool;
	const size_t size;
	std::atomic<intptr_t> nextAlloc;

public:
	MemoryPool(size_t poolsize) :
		size(poolsize + (512 - poolsize % MEMORYPOOL_GRANULARITY)),
		pool(operator new (poolsize + (512 - poolsize % MEMORYPOOL_GRANULARITY))) {
		nextAlloc.store(reinterpret_cast<intptr_t>(pool));
	}
	MemoryPool(const MemoryPool& other) = delete;
	~MemoryPool() {}

	template <class T>
	T* alloc(int n_elements = 1) {
		if (n_elements <= 0) return nullptr;

		size_t block_size = sizeof(T) * n_elements;
		if (block_size % ALIGNMENT != 0) {
			block_size += ALIGNMENT - (block_size % ALIGNMENT);
		}

		intptr_t maybe = nextAlloc.load();
		do {
			// check to make sure the space is big enough
			if (maybe - reinterpret_cast<intptr_t>(pool) + block_size > size) {
				return nullptr;
			}
		// Use atomics to make sure nobody tried to modify it while we were checking space constraints
		} while (!nextAlloc.compare_exchange_weak(maybe, maybe + block_size));
		return reinterpret_cast<T*>(maybe);
	}

	inline char* alloc_str(const char* str) {
		size_t str_size = strlen(str) + 1; // +1 for the null terminator
		char* result = alloc<char>(str_size);
		if (result == nullptr) return nullptr;
		strcpy(result, str);
		return result;
	}

	/** 'Free' everything allocated in this pool
	* Resets allocation pointer to the beginning of the pool.
	* All memory is free to be overwritten. Destructors will not be run.
	* Use with care.
	*/
	__forceinline void clear() {
		nextAlloc.store(reinterpret_cast<intptr_t>(pool));
	}

	/** Free everything allocated in this pool
	* Destructors will not be run.
	* Use with care.
	*/
	__forceinline void free() {
		operator delete(pool);
	}

	__forceinline size_t get_slack() {
		return size - (nextAlloc.load() - reinterpret_cast<intptr_t>(pool));
	}

	__forceinline size_t get_size() {
		return size;
	}
};

// I will probably separate these at some point for the sake of performance
typedef MemoryPool AtomicMemoryPool;

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
		T* result = (T*)std::align(nextAlloc);
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