#include "assetmanager.h"
#include "fileutil.h"

AssetManager AssetManager::singleton = AssetManager();

void AssetManager::store_raw(const char* filename, const void* asset, const std::type_info* type) {
	AssetEntry entry = {
		copy(filename),
		asset,
		type,
		0
	};
	auto result = assets.add(entry);
	if (!result) {
		assets.resize(assets.size() * 2);
		assets.add(entry);
	}
}

const AssetEntry* AssetManager::retrieve_raw(const char* filename) {
	for (AssetEntry& entry : assets) {
		if (strcmp(entry.filename, filename) == 0) {
			return &entry;
		}
	}
	return nullptr;
}