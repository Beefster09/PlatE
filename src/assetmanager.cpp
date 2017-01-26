#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include "cstrkey.h"
#include "assetmanager.h"
#include "fileutil.h"
#include "error.h"

namespace AssetManager {
	typedef std::unordered_map<cstr_key, AssetEntry> AssetMap;

	static AssetMap assets(1024);
	static std::string root_dir;

	static void free_asset(AssetEntry& entry);

	void store_raw(const char* filename, const void* asset, const std::type_index& type) {
		assets.insert(std::make_pair(cstr_key(filename, true), AssetEntry{ asset, type, 0 }));
	}

	const AssetEntry* retrieve_raw(const char* filename) {
		const cstr_key key(filename);

		AssetMap::const_iterator iter = assets.find(key);
		if (iter == assets.end()) return nullptr;
		return &(iter->second);
	}

	void free_asset(AssetEntry& entry) {
		if (entry.asset == nullptr) return;

		if (entry.type == typeid(GPU_Image)) {
			GPU_FreeImage(const_cast<GPU_Image*>((const GPU_Image*)entry.asset));
		}
		else {
			delete entry.asset;
		}
		entry.asset = nullptr;
	}

	void set_root_dir(const char* dir) {
		if (dir == nullptr || strlen(dir) == 0) {
			root_dir = "";
			return;
		}
		// remove leading slashes
		while (*dir == '/') ++dir;

		root_dir = dir;

		// remove trailing slashes
		while (root_dir.size() > 0 && root_dir[root_dir.size() - 1] == '/') root_dir.erase(root_dir.size() - 1);
	}

}

//DirContext::DirContext(const std::string& absolute) : DirContext(absolute.c_str()) {}
//DirContext::DirContext(const char* absolute) {
//	while (*absolute == '/') ++absolute; //skip leading slashes
//	int last_slash = 0;
//	// trim everything after the last slash (assume it's a file)
//	for (int i = 0; absolute[i] != 0; ++i) {
//		if (absolute[i] == '/') {
//			last_slash = i;
//		}
//	}
//	dir += '/';
//	dir.append(absolute, last_slash);
//}

Result<DirContext> DirContext::operator +(const char* path) {
	int len = strlen(path);
	if (len == 0) return *this;

#if __WIN32__
	{
		const char* drive;
		if ((drive = strstr(path, ":\\")) != nullptr) {
			if (drive > path) {
				char letter = toupper(*(drive - 1));
				if (letter >= 'A' && letter <= 'Z') {
					return Error(Errors::BadPath, "Attempt to access drive by letter.");
				}
			}
		}
	}
#endif

	int s = 0, e = 0;
	if (path[0] == '/') {
		if (strstr(path, "/../") != nullptr) {
			return Error(Errors::BadPath, "../ in absolute path");
		}

		while (path[0] == '/') ++path;

		DirContext absolute;
		
		int last_slash = 0;
		// trim everything after the last slash (assume it's a file)
		for (int i = 0; path[i] != 0; ++i) {
			if (path[i] == '/') {
				last_slash = i;
			}
		}

		absolute.dir.append(path, last_slash);
		return absolute;
	}
	else {
		DirContext result(*this);

		int last_slash = 0;
		// trim everything after the last slash (assume it's a file)
		for (int i = 0; path[i] != 0; ++i) {
			if (path[i] == '/') {
				last_slash = i;
			}
		}

		result.dir.append(path, last_slash);

		const char* match;
		while ((match = strstr(result.dir.c_str(), "/./")) != nullptr) {
			result.dir.erase(match - result.dir.c_str(), 2);
		}
		while ((match = strstr(result.dir.c_str(), "/../")) != nullptr) {
			int eraselen = 4;
			int i;
			for (i = match - result.dir.c_str() - 1; i >= 0; --i, ++eraselen) {
				if (result.dir[i] == '/') {
					break;
				}
			}
			if (i < 0) {
				return Error(Errors::BadPath, "Attempt to escape sandboxed directory with ../");
			}
			result.dir.erase(i, eraselen);
		}

		return result;
	}
}

Result<std::string> DirContext::resolve(const char* path) {
	if (strlen(path) == 0) return Error(Errors::BadPath, "No file given");

#if __WIN32__
	{
		const char* drive;
		if ((drive = strstr(path, ":\\")) != nullptr) {
			if (drive > path) {
				char letter = toupper(*(drive - 1));
				if (letter >= 'A' && letter <= 'Z') {
					return Error(Errors::BadPath, "Attempt to access drive by letter.");
				}
			}
		}
	}
#endif

	if (path[0] == '/') {
		if (strstr(path, "/../") != nullptr) {
			return Error(Errors::BadPath, "../ in absolute path");
		}

		std::string result(AssetManager::root_dir);

		result += path;

		return result;
	}
	else {
		std::string result(AssetManager::root_dir);

		result += '/';
		result += path;

		const char* match;
		while ((match = strstr(result.c_str(), "/./")) != nullptr) {
			result.erase(match - result.c_str(), 2);
		}
		while ((match = strstr(result.c_str(), "/../")) != nullptr) {
			int eraselen = 4;
			int i;
			for (i = match - result.c_str() - 1; i >= 0; --i, ++eraselen) {
				if (result[i] == '/') {
					break;
				}
			}
			if (i < 0) {
				return Error(Errors::BadPath, "Attempt to escape sandboxed directory with ../");
			}
			result.erase(i, eraselen);
		}

		return result;
	}
}
