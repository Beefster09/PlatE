
#include "level.h"
#include "storage.h"
#include "error.h"
#include "fileutil.h"

__forceinline static Result<const Level*> read_level(FILE* stream, MemoryPool& pool,
	uint32_t namelen, AABB boundary, uint32_t n_tilemaps, uint32_t n_objects,
	uint32_t n_entities, uint32_t n_areas, uint32_t n_edge_triggers);

Result<const Level*> load_level(const char* filename) {
	// TODO/later managed assets
	auto file = open(filename, "rb");

	if (!file) {
		return file.err;
	}
	FILE* stream = file;

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

	if (poolsize > LEVEL_MAX_SIZE) {
		fclose(stream);
		return Errors::LevelDataTooLarge;
	}

	printf("Number of bytes needed for level data: %zd\n", poolsize);
	MemoryPool pool(poolsize);

	auto result = read_level(stream, pool,
		namelen, boundary, n_tilemaps, n_objects, n_entities, n_areas, n_edge_triggers);

	printf("Read level data with %zd/%zd bytes of slack in memory pool\n", pool.get_slack(), pool.get_size());

	if (!result) {
		// Clean up from the error
		pool.free();
	}

	fclose(stream);

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
			assert(false && "NOT IMPLEMENTED!");
		}

		SceneObject* objects = pool.alloc<SceneObject>(n_objects);
		for (int i = 0; i < n_objects; ++i) {
			auto& obj = objects[i];

			uint32_t texnamelen = read<uint32_t>(stream);
			/*obj.clip =*/ read<SDL_Rect>(stream);
			obj.display = read<Vector2>(stream);
			obj.position = read<Vector2>(stream);
			obj.z_order = read<int32_t>(stream);
			obj.rotation = read<float>(stream);
			obj.scale = read<Vector2>(stream);
			uint32_t n_colliders = read<uint32_t>(stream);

			{
				// TODO: managed textures
				char texname[1024];
				fread(texname, 1, texnamelen, stream);
				texname[texnamelen] = 0;

				// TODO: Load texture here
				//obj.texture = nullptr;
			}

			/*obj.collision =*/ read_colliders(stream, n_colliders, pool);
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