#include "assetmanager.h"
#include "fileutil.h"
#include "error.h"
#include "cstrkey.h"

typedef std::unordered_map<cstr_key, AssetEntry> AssetMap;

AssetManager AssetManager::singleton;

void AssetManager::store_raw(const char* filename, const void* asset, const std::type_index& type) {
	assets.insert(std::make_pair(cstr_key(filename, true), AssetEntry{ asset, type, 0 }));
}

const AssetEntry* AssetManager::retrieve_raw(const char* filename) const {
	const cstr_key key(filename);

	AssetMap::const_iterator iter = assets.find(key);
	if (iter == assets.end()) return nullptr;
	return &(iter->second);
}

static void free_asset(AssetEntry& entry);

AssetManager::~AssetManager() {
	for (auto& pair : assets) {
		free_asset(pair.second);
	}
}

void free_asset(AssetEntry& entry) {
	if (entry.asset == nullptr) return;

	if (entry.type == typeid(GPU_Image)) {
		GPU_FreeImage(const_cast<GPU_Image*>((const GPU_Image*) entry.asset));
	}
	else {
		delete entry.asset;
	}
	entry.asset = nullptr;
}