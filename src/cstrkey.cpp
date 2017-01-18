#include <cstdint>
#include "cstrkey.h"
#include "util.h"
#include "error.h"

/// Constants needed for the FNV-1 hashing algorithm - 32 and 64 bit variants
constexpr size_t HASHBASE = sizeof(size_t) == sizeof(uint64_t) ? 14695981039346656037 : 2166136261;
constexpr size_t HASHPRIME = sizeof(size_t) == sizeof(uint64_t) ? 1099511628211 : 16777619;

cstr_key::cstr_key(const char* str, bool copy_cstr) {
	if (copy_cstr) {
		cstr = copy(str);
		owns_buffer = true;
	}
	else {
		cstr = str;
		owns_buffer = false;
	}

	hash = HASHBASE;
	for (const char* c = cstr; *c; ++c) {
		hash *= HASHPRIME;
		hash ^= *c;
	}
}

cstr_key::cstr_key(const cstr_key& other) {
	LOG_VERBOSE("Warning: cstr_key implicit copy\n");
	cstr = copy(other.cstr);
	owns_buffer = true;
	hash = other.hash;
}

cstr_key::~cstr_key() {
	if (owns_buffer) {
		delete[] cstr;
	}
}

cstr_key::cstr_key() {
	cstr = nullptr;
	hash = 0;
	owns_buffer = false;
}

cstr_key::cstr_key(cstr_key&& other) {
	cstr = other.cstr;
	hash = other.hash;
	if (other.owns_buffer) { // steal ownership
		owns_buffer = true;
		other.owns_buffer = false;
	}
	else {
		owns_buffer = false;
	}
}

void cstr_key::operator = (cstr_key&& other) {
	if (owns_buffer) delete[] cstr;
	cstr = other.cstr;
	hash = other.hash;
	if (other.owns_buffer) { // steal ownership
		owns_buffer = true;
		other.owns_buffer = false;
	}
	else {
		owns_buffer = false;
	}
}

void cstr_key::copy_if_unowned() {
	if (!owns_buffer) {
		cstr = copy(cstr);
		owns_buffer = true;
	}
}