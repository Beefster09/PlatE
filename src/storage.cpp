
#include "storage.h"
#include <cstring>

MemoryPool::MemoryPool(size_t poolsize) {
	pool = new uint8_t[poolsize];
	nextAlloc = pool;
	size = poolsize;
}

char* MemoryPool::alloc_str(const char* str) {
	size_t str_size = strlen(str) + 1; // +1 for the null terminator
	if ((intptr_t)nextAlloc - (intptr_t)pool + str_size > size) return nullptr;
	char* result = (char*)nextAlloc;
	strcpy(result, str);
	nextAlloc = result + str_size;
	return result;
}