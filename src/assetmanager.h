#pragma once

#include "fileutil.h"
#include <unordered_map>
#include <string>
#include <typeinfo>
#include <typeindex>

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

	__forceinline bool operator == (const cstr_key& other) const {
		if (cstr == other.cstr) return true;
		if (hash != other.hash) return false;
		return strcmp(cstr, other.cstr);
	}
	__forceinline size_t get_hash() const { return hash; }
	__forceinline const char* c_str() const { return cstr; }

	void copy_if_unowned();
};

template<>
struct std::hash<cstr_key> {
	__forceinline size_t operator() (const cstr_key& key) const { return key.get_hash(); }
};

struct AssetEntry {
	const void* asset;
	std::type_index type;
	uint32_t flags;
};

class AssetManager {
private:
	std::unordered_map<cstr_key, AssetEntry> assets;

	AssetManager() : assets(1024) {}

	void store_raw(const char* filename, const void* asset, const std::type_index& type);
	const AssetEntry* retrieve_raw(const char* filename);

	static AssetManager singleton;
public:
	AssetManager(const AssetManager& other) = delete;
	AssetManager(AssetManager&& rvalue) = delete;

	~AssetManager();

	inline static AssetManager& get() { return singleton; }

	// Garbage collect assets
	void gc();

	template <class T>
	__forceinline void store(const char* filename, const T* asset) {
		store_raw(filename, asset, typeid(T));
	}

	template <class T>
	const T* retrieve(const char* filename) {
		const AssetEntry* asset = retrieve_raw(filename);
		if (asset == nullptr) return nullptr;
		if (asset->type == typeid(T)) {
			return static_cast<const T*>(asset->asset);
		}
		else return nullptr;
	}

};
