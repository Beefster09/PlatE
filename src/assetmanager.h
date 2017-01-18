#pragma once

#include "fileutil.h"
#include <unordered_map>
#include <string>
#include <typeinfo>
#include <typeindex>
#include "cstrkey.h"


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
	const AssetEntry* retrieve_raw(const char* filename) const;

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
	const T* retrieve(const char* filename) const {
		const AssetEntry* asset = retrieve_raw(filename);
		if (asset == nullptr) return nullptr; // missing
		if (asset->type == typeid(T)) {
			return static_cast<const T*>(asset->asset);
		}
		else return nullptr; // wrong type
	}

};
