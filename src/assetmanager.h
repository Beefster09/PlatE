#pragma once

#include <vector>
#include <string>
#include <typeinfo>
#include <typeindex>
#include "result.h"

namespace Errors {
	const error_data
		BadPath = { 101, "Invalid path" };
}

/// Allows for relative asset referencing in a cross-platform way
/// Will deal with forward/backward-ness of slashes appropriately
class DirContext {
private:
	std::string dir;
public:
	DirContext() {}
	//DirContext(const std::string& absolute);
	//DirContext(const char* absolute);
	DirContext(const DirContext&) = default;
	DirContext(DirContext&&) = default;

	/// Appends directories from a path
	/// Each directory will be pushed individually, the basename will be ignored
	/// If the path begins with a single slash, the path is assumed to be absolute from the asset root
	/// Any attempts to reach above the asset root will result in an error
	/// On Windows, attempts to access drives by letter will result in an error
	Result<DirContext> operator +(const char* path);
		
	/// Resolves an engine-absolute path relative to the current context
	Result<std::string> resolve(const char* path); // needs to return a std::string to avoid dangling pointer
};

namespace AssetManager {
	struct AssetEntry {
		const void* asset;
		std::type_index type;
		uint32_t flags;
	};

	void store_raw(const char* filename, const void* asset, const std::type_index& type);
	const AssetEntry* retrieve_raw(const char* filename);

	void set_root_dir(const char* dir);

	// Asset Garbage Collection (slow- should probably only be allowed to be triggered on loading screens)
	void gc();

	template <class T>
	__forceinline void store(const char* filename, const T* asset) {
		store_raw(filename, asset, typeid(T));
	}

	template <class T>
	const T* retrieve(const char* filename) {
		const AssetEntry* asset = retrieve_raw(filename);
		if (asset == nullptr) return nullptr; // missing
		if (asset->type == typeid(T)) {
			return static_cast<const T*>(asset->asset);
		}
		else return nullptr; // wrong type
	}

};
