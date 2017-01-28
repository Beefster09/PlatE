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

	bool set_root_dir(const char* dir) {
		if (dir == nullptr || strlen(dir) == 0) {
			root_dir = "";
			return false;
		}
		// remove leading slashes
		while (*dir == '/') ++dir;

		root_dir = dir;

		// remove trailing slashes
		while (root_dir.size() > 0 && root_dir[root_dir.size() - 1] == '/') root_dir.erase(root_dir.size() - 1);

		return true;
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

// resolves ./ and ../ from a path
static inline Result<> resolve_relative(std::string& path) {
	size_t match;

	// remove runs of multiple slashes
	match = 0;
	while ((match = path.find('/', match)) != std::string::npos) {
		size_t notslash = path.find_first_not_of('/', ++match);
		if (notslash == std::string::npos) {
			path.erase(match);
			break;
		}
		if (notslash != match) {
			path.erase(match, notslash - match);
		}
	}

	// ../ resolution
	match = 0;
	while ((match = path.find("../", match)) != std::string::npos) {
		if (match == 0) {
			return Error(Errors::BadPath, "Attempt to escape sandboxed directory with ../");
		}
		if (path[match - 1] != '/') {
			// False alarm- it was a regular directory that just happened to end with two dots
			match += 3;
			continue;
		}
		size_t end = match + 3;
		size_t start = path.rfind('/', match - 2);
		if (start == std::string::npos) {
			start = 0;
		}
		path.erase(start, end - start);
		match = start;
	}

	match = 0;
	while ((match = path.find("./", match)) != std::string::npos) {
		// Make sure it's not just a directory name that just happened to end with a dot
		if (match == 0 || path[match - 1] == '/') {
			path.erase(match, 2);
		}
		else {
			match += 2;
		}
	}

	return Result<>::success;
}

Result<DirContext> DirContext::operator +(const char* path) const {
	if (*path == 0) return *this;

	// No directories in path
	if (strchr(path, '/') == nullptr) return *this;

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

		result.dir += '/';
		result.dir.append(path, last_slash);

		check_void(resolve_relative(result.dir));

		return result;
	}
}

Result<std::string> DirContext::resolve(const char* path) const {
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

		// Technically, Windows won't complain about /, but just to be safe...
#if __WIN32__
		for (char& chr : result) {
			if (chr == '/') chr = '\\';
		}
#endif

		return result;
	}
	else {
		std::string result(dir);
		if(result.size() > 0) result += '/';
		result += path;

		check_void(resolve_relative(result));

		result.insert(0, "/");
		result.insert(0, AssetManager::root_dir);

		// Technically, Windows won't complain about /, but just to be safe...
#if __WIN32__
		for (char& chr : result) {
			if (chr == '/') chr = '\\';
		}
#endif

		return result;
	}
}
