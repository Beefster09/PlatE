#pragma once

#include "fileutil.h"
#include "storage.h"
#include <typeinfo>

struct AssetEntry {
	const char* filename;
	const void* asset;
	const std::type_info* type;
	uint32_t gc_flags;
};

class AssetManager {
private:
	DenseBucket<AssetEntry> assets;

	AssetManager() : assets(1024) {}

	void store_raw(const char* filename, const void* asset, const std::type_info* type);
	const AssetEntry* retrieve_raw(const char* filename);

	static AssetManager singleton;
public:
	inline static AssetManager& get() { return singleton; }

	// Garbage collect assets
	void gc();

	template <class T>
	__forceinline void store(const char* filename, const T* asset) {
		store_raw(filename, asset, &typeid(T));
	}

	template <class T>
	const T* retrieve(const char* filename) {
		const AssetEntry* asset = retrieve_raw(filename);
		if (asset == nullptr) return nullptr;
		if (*asset->type == typeid(T)) {
			return static_cast<const T*>(asset->asset);
		}
		else return nullptr;
	}

};
