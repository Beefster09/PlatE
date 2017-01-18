#include "storage.h"
#include "tileset.h"
#include "error.h"
#include "result.h"
#include "fileutil.h"
#include "assetmanager.h"
#include <cstdio>
#include <cerrno>
#include <cstring>

__forceinline static Result<Tileset*> read_tileset(FILE* stream, MemoryPool& pool,
	uint32_t namelen, uint32_t texnamelen, uint32_t n_tiles);

Result<const Tileset*> load_tileset(const char* filename) {
	auto& aman = AssetManager::get();
	{
		const Tileset* maybe = aman.retrieve<Tileset>(filename);
		if (maybe != nullptr) return maybe;
	}

	auto file = open(filename, "rb");

	if (!file) {
		return file.err;
	}
	FILE* stream = file;

	// Check the magic number
	{
		char headbytes[TILESET_MAGIC_NUMBER_LENGTH + 1];
		headbytes[TILESET_MAGIC_NUMBER_LENGTH] = 0;
		if (fread(headbytes, 1, TILESET_MAGIC_NUMBER_LENGTH, stream) != TILESET_MAGIC_NUMBER_LENGTH ||
			strcmp(headbytes, TILESET_MAGIC_NUMBER) != 0) {
			return Errors::InvalidTilesetHeader;
		}
	}

	// Read the header to determine how much we need to allocate in the memory pool
	uint32_t namelen, texnamelen, n_tiles,
		tn_tileframes, tn_hitboxes, tn_vertices;
	uint16_t tile_w, tile_h;

	try {
		namelen = read<uint32_t>(stream);
		texnamelen = read<uint32_t>(stream);
		tile_w = read<uint16_t>(stream);
		tile_h = read<uint16_t>(stream);
		n_tiles = read<uint32_t>(stream);
		tn_tileframes = read<uint32_t>(stream);
		tn_hitboxes = read<uint32_t>(stream);
		tn_vertices = read<uint32_t>(stream);
	}
	catch (Error& err) {
		fclose(stream);
		return err;
	}

	size_t poolsize =
		sizeof(Tileset) +
		n_tiles * sizeof(Tile) +
		tn_tileframes * sizeof(TileFrame) +
		tn_hitboxes * sizeof(Hitbox) +
		tn_vertices * sizeof(Vector2) +
		namelen + 1;

	LOG_VERBOSE("Number of bytes needed for tileset data: %zd\n", poolsize);
	MemoryPool pool(poolsize);

	auto result = read_tileset(stream, pool, namelen, texnamelen, n_tiles);

	LOG_VERBOSE("Read tileset data with %zd/%zd bytes of slack in memory pool\n", pool.get_slack(), pool.get_size());

	fclose(stream);

	if (!result) {
		// clean up
		pool.free();
		return result.err;
	}
	else {
		Tileset* tileset = result.value;

		tileset->tile_width = tile_w;
		tileset->tile_height = tile_h;

		aman.store(filename, tileset);

		return tileset;
	}
}

__forceinline static Result<Tileset*> read_tileset(FILE* stream, MemoryPool& pool,
	uint32_t namelen, uint32_t texnamelen, uint32_t n_tiles) {
	try {
		Tileset* tileset = pool.alloc<Tileset>();

		tileset->name = read_string(stream, namelen, pool);
		tileset->tilesheet = read_referenced_texture(stream, texnamelen);

		Tile* tiles = pool.alloc<Tile>(n_tiles);
		for (int i = 0; i < n_tiles; ++i) {
			auto& tile = tiles[i];

			uint32_t n_frames = read<uint32_t>(stream);
			uint32_t n_properties = read<uint32_t>(stream);

			auto sType = read<Tile::Solidity::Type>(stream);
			tile.solidity.type = sType;
			switch (sType) {
			case Tile::Solidity::None:
				break;
			case Tile::Solidity::Partial:
				tile.solidity.partial.position = read<float>(stream);
				tile.solidity.partial.vertical = read<bool>(stream);
				tile.solidity.partial.topleft = read<bool>(stream);
				break;
			case Tile::Solidity::Slope:
				tile.solidity.slope.position = read<float>(stream);
				tile.solidity.slope.slope = read<float>(stream);
				tile.solidity.slope.above = read<bool>(stream);
				break;
			case Tile::Solidity::Complex:
				tile.solidity.complex = read_hitbox(stream, pool);
				break;
			default:
				break;
			}

			TileFrame* frames = pool.alloc<TileFrame>(n_frames);
			for (int j = 0; j < n_frames; ++j) {
				frames[j] = {
					read<uint16_t>(stream),
					read<uint16_t>(stream),
					read<float>(stream),
					read<uint8_t>(stream)
				};
				frames[j].flip &= 0x03;
			}

			new(&tile.animation) Array<const TileFrame>(frames, n_frames);
		}

		new(&tileset->tile_data) Array<const Tile>(tiles, n_tiles);

		return tileset;
	}
	catch (Error& err) {
		return err;
	}
}

Result<const Tileset*> read_referenced_tileset(FILE* stream, uint32_t len) {
	char fn[1024];
	fread(fn, 1, len, stream);
	fn[len] = 0;

	return load_tileset(fn);
}