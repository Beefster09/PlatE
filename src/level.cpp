
#include "level.h"
#include "storage.h"
#include "error.h"
#include "fileutil.h"
#include "assetmanager.h"
#include "SDL_gpu.h"

__forceinline static Result<const Level*> read_level(FILE* stream, MemoryPool& pool,
	uint32_t namelen, AABB boundary, uint32_t n_tilemaps, uint32_t n_objects,
	uint32_t n_entities, uint32_t n_areas, uint32_t n_edge_triggers);

Result<const Level*> load_level(const char* filename) {
	auto& aman = AssetManager::get();
	{
		const Level* maybe = aman.retrieve<Level>(filename);
		if (maybe != nullptr) return maybe;
	}

	auto file = open(filename, "rb");

	if (!file) {
		return file.err;
	}
	FILE* stream = file;

	size_t filesize = size(file);

	// Check the magic number
	{
		char headbytes[LEVEL_MAGIC_NUMBER_LENGTH + 1];
		headbytes[LEVEL_MAGIC_NUMBER_LENGTH] = 0;
		if (fread(headbytes, 1, LEVEL_MAGIC_NUMBER_LENGTH, stream) != LEVEL_MAGIC_NUMBER_LENGTH ||
			strcmp(headbytes, LEVEL_MAGIC_NUMBER) != 0) {
			return Errors::InvalidLevelHeader;
		}
	}

	uint32_t namelen, n_tilemaps, n_objects, n_entities, n_areas, n_edge_triggers,
		tn_tiles, tn_colliders, tn_nested_hitboxes;
	AABB boundary;

	try {
		namelen = read<uint32_t>(stream);
		boundary = read<AABB>(stream);
		n_tilemaps = read<uint32_t>(stream);
		n_objects = read<uint32_t>(stream);
		n_entities = read<uint32_t>(stream);
		n_areas = read<uint32_t>(stream);
		n_edge_triggers = read<uint32_t>(stream);
		tn_tiles = read<uint32_t>(stream);
		tn_colliders = read<uint32_t>(stream);
		tn_nested_hitboxes = read<uint32_t>(stream);
	}
	catch (Error& err) {
		fclose(stream);
		return err;
	}

	size_t poolsize =
		sizeof(Level) +
		namelen + 1 + (8 - (namelen + 1) % 8) +
		n_tilemaps * sizeof(Tilemap) +
		n_objects * sizeof(SceneObject) +
		n_entities * sizeof(EntitySpawnPoint) +
		n_areas * sizeof(LevelArea) +
		n_edge_triggers * sizeof(EdgeTrigger) +
		tn_tiles * sizeof(uint16_t) +
		tn_colliders * sizeof(Collider) +
		tn_nested_hitboxes * sizeof(Hitbox);

	if (LEVEL_BALLPARK_CHECKED && !ballpark(poolsize, filesize)) {
		fprintf(stderr, "Size suggested by header (%zd bytes) is significantly different than the file's size. (%zd bytes) "
			"Perhaps the asset is corrupted or incorrectly formatted.\n", poolsize, filesize);
		if (LEVEL_BALLPARK_REQUIRED) return Errors::InvalidLevelHeaderSizes;
	}

	LOG_VERBOSE("Number of bytes needed for level data: %zd\n", poolsize);
	MemoryPool pool(poolsize);

	auto result = read_level(stream, pool,
		namelen, boundary, n_tilemaps, n_objects, n_entities, n_areas, n_edge_triggers);

	LOG_VERBOSE("Read level data with %zd/%zd bytes of slack in memory pool\n", pool.get_slack(), pool.get_size());

	fclose(stream);

	if (result) {
		aman.store(filename, result.value);
	}
	else {
		// Clean up from the error
		pool.free();
	}

	return result;
}

__forceinline static Result<const Level*> read_level(FILE* stream, MemoryPool& pool,
	uint32_t namelen, AABB boundary, uint32_t n_tilemaps, uint32_t n_objects,
	uint32_t n_entities, uint32_t n_areas, uint32_t n_edge_triggers) {

	try {
		Level* level = pool.alloc<Level>();

		level->name = read_string(stream, namelen, pool);
		level->boundary = boundary;

		Tilemap* tilemaps = pool.alloc<Tilemap>(n_tilemaps);
		for (int i = 0; i < n_tilemaps; ++i) {
			auto& tmap = tilemaps[i];

			uint32_t tilesetnamelen = read<uint32_t>(stream);
			uint32_t width = read<uint32_t>(stream);
			uint32_t height = read<uint32_t>(stream);
			uint32_t area = width * height;

			tmap.z_order = read<int32_t>(stream);
			tmap.offset = read<Vector2>(stream);
			tmap.scale = read<Vector2>(stream);
			tmap.parallax = read<Vector2>(stream);
			tmap.solid = read<bool>(stream);

			auto tileset = read_referenced_tileset(stream, tilesetnamelen);
			if (tileset) {
				tmap.tileset = tileset;
			}
			else {
				ERR_RELEASE("Unable to load referenced tileset (%s).\n", tileset.err.description);
				return tileset.err;
			}

			uint16_t* tiles = pool.alloc<uint16_t>(area);
			for (int xy = 0; xy < area; ++xy) {
				tiles[xy] = read<uint16_t>(stream);
			}
			tmap.tiles = Array2D<uint16_t>(tiles, width, height);
		}

		level->layers = Array<const Tilemap>(tilemaps, n_tilemaps);

		SceneObject* objects = pool.alloc<SceneObject>(n_objects);
		for (int i = 0; i < n_objects; ++i) {
			auto& obj = objects[i];

			uint32_t texnamelen = read<uint32_t>(stream);
			obj.display = read<Vector2>(stream);
			obj.position = read<Vector2>(stream);
			obj.z_order = read<int32_t>(stream);
			obj.rotation = read<float>(stream);
			obj.scale = read<Vector2>(stream);
			uint32_t n_colliders = read<uint32_t>(stream);

			auto sprite = read_referenced_sprite(stream, texnamelen);
			if (sprite) {
				obj.sprite = sprite;
			}
			else {
				ERR_RELEASE("Unable to load referenced sprite (%s).\n", sprite.err.description);
				return sprite.err;
			}
		}

		EntitySpawnPoint* entities = pool.alloc<EntitySpawnPoint>(n_entities);
		for (int i = 0; i < n_entities; ++i) {
			assert(false && "NOT IMPLEMENTED!");
		}

		LevelArea* areas = pool.alloc<LevelArea>(n_areas);
		for (int i = 0; i < n_areas; ++i) {
			assert(false && "NOT IMPLEMENTED!");
		}

		EdgeTrigger* edge_triggers = pool.alloc<EdgeTrigger>(n_edge_triggers);
		for (int i = 0; i < n_edge_triggers; ++i) {
			assert(false && "NOT IMPLEMENTED!");
		}

		level->objects = Array<const SceneObject>(objects, n_objects);

		return level;
	}
	catch (Error& err) {
		return err;
	}
}

#define TILE_RENDER_BATCH_SIZE 500

void render_tilemap(GPU_Target* context, const Tilemap* map) {
	const Tileset* tset = map->tileset;
	GPU_Image* texture = tset->tilesheet;
	auto& tdata = tset->tile_data;
	auto tiles = map->tiles;
	auto w = tiles.width();
	auto h = tiles.height();
	float width = static_cast<float>(tset->tile_width);
	float height = static_cast<float>(tset->tile_height);
	
	GPU_Rect dest = { 0.f, 0.f, width, height };
	GPU_Rect src = { 0.f, 0.f, width, height };

	for (size_t x_ind = 0; x_ind < w; ++x_ind) {
		dest.x = x_ind * width + map->offset.x;
		for (size_t y_ind = 0; y_ind < h; ++y_ind) {
			uint16_t t = tiles(x_ind, y_ind);
			if (t != TILE_BLANK) {
				dest.y = y_ind * height + map->offset.y;

				--t; // Because tiles are 1-indexed

				// TODO: tile animation
				auto& frame = tdata[t].animation[0];
				src.x = frame.x_ind * width;
				src.y = frame.y_ind * height;

				GPU_BlitRectX(texture, &src, context, &dest, 0.f, 0.f, 0.f, GPU_FLIP_NONE);
			}
		}
	}
}