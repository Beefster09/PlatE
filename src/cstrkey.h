#pragma once

#include <utility>

/// Needed for performance reasons to account for implementation discrepancies
/// Also helps to avoid some less than ideal std::string hidden allocations
class cstr_key {
private:
	const char* cstr;
	size_t hash;
	bool owns_buffer;

public:
	cstr_key();
	~cstr_key();
	cstr_key(cstr_key&& other);
	cstr_key(const cstr_key& other);
	explicit cstr_key(const char* str, bool copy_cstr = false);
	void operator = (cstr_key&& other); // move assignment with ownership stealing

	inline bool operator == (const cstr_key& other) const {
		return strcmp(cstr, other.cstr) == 0;
	}
	__forceinline size_t get_hash() const { return hash; }
	__forceinline const char* c_str() const { return cstr; }

	void copy_if_unowned();
};

template<>
struct std::hash<cstr_key> {
	__forceinline size_t operator() (const cstr_key& key) const { return key.get_hash(); }
};