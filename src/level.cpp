
#include "level.h"
#include "storage.h"
#include "error.h"
#include "fileutil.h"
#include "assetmanager.h"
#include "SDL_gpu.h"

__forceinline static Result<const Level*> read_level(FILE* stream, MemoryPool& pool,
	uint32_t namelen, AABB boundary, uint32_t n_tilemaps, uint32_t n_objects,
	uint32_t n_entities, uint32_t n_areas, uint32_t n_edge_triggers,
	const DirContext& context);

Result<const Level*> load_level(const char* filename, const DirContext& context) {
	std::string realfile;
	check_assign(realfile, context.resolve(filename));

	{
		const Level* maybe = AssetManager::retrieve<Level>(realfile.c_str());
		if (maybe != nullptr) return maybe;
	}

	auto file = open(realfile.c_str(), "rb");

	if (!file) {
		return file.err;
	}
	FILE* stream = file;

	size_t filesize = size(file);

	if (!check_header(stream, LEVEL_MAGIC_NUMBER)) {
		return Errors::InvalidLevelHeader;
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

	check_assign_ref(const DirContext& subcontext, context + filename, sctx);
	auto result = read_level(stream, pool,
		namelen, boundary, n_tilemaps, n_objects, n_entities, n_areas, n_edge_triggers,
		subcontext);

	LOG_VERBOSE("Read level data with %zd/%zd bytes of slack in memory pool\n", pool.get_slack(), pool.get_size());

	fclose(stream);

	if (result) {
		AssetManager::store(filename, result.value);
	}
	else {
		// Clean up from the error
		pool.free();
	}

	return result;
}

__forceinline static Result<const Level*> read_level(FILE* stream, MemoryPool& pool,
	uint32_t namelen, AABB boundary, uint32_t n_tilemaps, uint32_t n_objects,
	uint32_t n_entities, uint32_t n_areas, uint32_t n_edge_triggers,
	const DirContext& context) {

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

			auto tileset = read_referenced_tileset(stream, tilesetnamelen, context);
			if (tileset) {
				tmap.tileset = tileset;
			}
			else {
				ERR_RELEASE("Unable to load referenced tileset (%s).\n", std::to_string(tileset.err).c_str());
				return tileset.err;
			}

			uint16_t* tiles = pool.alloc<uint16_t>(area);
			for (int xy = 0; xy < area; ++xy) {
				tiles[xy] = read<uint16_t>(stream);
			}
			new(&tmap.tiles) Array2D<uint16_t>(tiles, width, height);
		}

		new(&level->layers) Array<const Tilemap>(tilemaps, n_tilemaps);

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

			auto sprite = read_referenced_sprite(stream, texnamelen, context);
			if (sprite) {
				obj.sprite = sprite;
			}
			else {
				ERR_RELEASE("Unable to load referenced sprite (%s).\n", std::to_string(sprite.err).c_str());
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

		new(&level->objects) Array<const SceneObject>(objects, n_objects);

		return level;
	}
	catch (Error& err) {
		return err;
	}
}

void render_tilemap(GPU_Target* context, const Tilemap* map) {
	const Tileset* tset = map->tileset;
	GPU_Image* texture = tset->tilesheet;
	auto& tdata = tset->tile_data;
	auto& tiles = map->tiles;
	auto w = tiles.width();
	auto h = tiles.height();
	float width = static_cast<float>(tset->tile_width);
	float height = static_cast<float>(tset->tile_height);
	uint16_t n_tiles = tset->tile_data.size();
	
	GPU_Rect dest = { 0.f, 0.f, width * fabsf(map->scale.x), height * fabsf(map->scale.y)};
	GPU_Rect src = { 0.f, 0.f, width, height };

	for (size_t x_ind = 0; x_ind < w; ++x_ind) {
		dest.x = x_ind * width + map->offset.x;
		for (size_t y_ind = 0; y_ind < h; ++y_ind) {
			uint16_t t = tiles(x_ind, y_ind);
			if (t != TILE_BLANK) {
				if (t > n_tiles) {
					assert(false);
					ERR("Tile index out of bounds (%hd > %hd)\n", t, n_tiles);
					continue;
				}
				dest.y = y_ind * height + map->offset.y;

				--t; // Because tiles are 1-indexed

				// TODO: tile animation
				auto& frame = tdata[t].animation[0];
				src.x = frame.x_ind * width;
				src.y = frame.y_ind * height;

				GPU_BlitRectX(texture, &src, context, &dest, 0.f, 0.f, 0.f, frame.flip);
			}
		}
	}
}

LevelInstance* instantiate_level(const Level* level) {
	size_t poolsize = sizeof(LevelInstance);
	size_t n_layers = level->layers.size();

	for (auto& layer : level->layers) {
		poolsize += sizeof(uint16_t) * layer.tiles.size();
		poolsize += sizeof(TileAnimationState) * layer.tileset->tile_data.size();
	}
	poolsize += (sizeof(Tilemap) + sizeof(Array<TileAnimationState>)) * n_layers;

	MemoryPool pool(poolsize);

	LevelInstance* inst = pool.alloc<LevelInstance>();

	inst->base = level;

	Tilemap* layers = pool.alloc<Tilemap>(n_layers);
	Array<TileAnimationState>* anim_states = pool.alloc<Array<TileAnimationState>>(n_layers);
	for (int i = 0; i < n_layers; ++i) {
		auto& layer = layers[i];
		const auto& orig = level->layers[i];
		size_t mapsize = orig.tiles.size();

		uint16_t* tilemap_raw = pool.alloc<uint16_t>(mapsize);

		memcpy(tilemap_raw, orig.tiles.data(), mapsize * sizeof(uint16_t));

		new(&layer.tiles) Array2D<uint16_t>(tilemap_raw, orig.tiles.width(), orig.tiles.height());
		layer.tileset = orig.tileset;
		layer.offset = orig.offset;
		layer.parallax = orig.parallax;
		layer.scale = orig.scale;
		layer.solid = orig.solid;
		layer.z_order = orig.z_order;

		size_t n_tilestates = orig.tileset->tile_data.size();
		TileAnimationState* anim_state = pool.alloc<TileAnimationState>(n_tilestates);

		const Tile* tile = orig.tileset->tile_data;
		for (int j = 0; j < n_tilestates; ++j, ++tile) {
			anim_state[j] = {
				tile,
				0.f,
				0
			};
		}

		new(&anim_states[i]) Array<TileAnimationState>(anim_state, n_tilestates);
	}

	new(&inst->layers) Array<Tilemap>(layers, n_layers);
	new(&inst->anim_state) Array<Array<TileAnimationState>>(anim_states, n_layers);

	return inst;
}

TileRange tiles_in(const Tilemap& map, const AABB& region) {
	AABB mregion = region - map.offset;
	float w = map.tileset->tile_width;
	float h = map.tileset->tile_height;
	mregion.left /= w;
	mregion.right /= w;
	mregion.top /= h;
	mregion.bottom /= h;

	TileRange range = {
		0,
		static_cast<uint16_t>(map.tiles.width() - 1),
		0,
		static_cast<uint16_t>(map.tiles.height() - 1)
	};

	if (mregion.left > range.left) {
		range.left = static_cast<uint16_t>(floorf(mregion.left));
	}
	if (mregion.right < range.right) {
		range.right = static_cast<uint16_t>(ceilf(mregion.right));
	}
	if (mregion.top > range.top) {
		range.top = static_cast<uint16_t>(floorf(mregion.top));
	}
	if (mregion.bottom < range.bottom) {
		range.bottom = static_cast<uint16_t>(ceilf(mregion.bottom));
	}

	return range;
}

bool entity_tilemap_collision(const Entity* e, const Tilemap& map) {
	if (!e->solid) return false;

	const Hitbox& hitbox = e->animation->solidity.hitbox;
	if (hitbox.type == Hitbox::BOX && (float_eq(e->rotation, 0.f) || e->animation->solidity.fixed)) {
		auto range = tiles_in(map, e->get_transform() * hitbox.box);
		float w = map.tileset->tile_width;
		float h = map.tileset->tile_height;

		for (int x = range.left; x <= range.right; ++x) {
			for (int y = range.top; y <= range.bottom; ++y) {
				uint16_t t_ind = map.tiles(x, y);
				if (t_ind == TILE_BLANK) continue;

				const Tile& tile = map.tileset->tile_data[t_ind - 1];

				if (tile.solidity.type == Tile::Solidity::None) {
					continue;
				}

				Vector2 offset = {
					fmaf((float) x, w, map.offset.x),
					fmaf((float) y, h, map.offset.y)
				};

				switch (tile.solidity.type) {
				case Tile::Solidity::Full:
					return true;
				case Tile::Solidity::Partial:
				{
					const auto& partial = tile.solidity.partial;
					if (partial.vertical) {
						if (partial.topleft) {
							if (hitbox.box.top < offset.y + partial.position ||
								hitbox.box.bottom > offset.y ||
								hitbox.box.left < offset.x + w ||
								hitbox.box.right > offset.x) {
								return true;
							}
						}
						else {
							if (hitbox.box.top < offset.y + h||
								hitbox.box.bottom > offset.y + partial.position ||
								hitbox.box.left < offset.x + w ||
								hitbox.box.right > offset.x) {
								return true;
							}
						}
					}
					else {
						if (partial.topleft) {
							if (hitbox.box.top < offset.y + h ||
								hitbox.box.bottom > offset.y ||
								hitbox.box.left < offset.x + partial.position||
								hitbox.box.right > offset.x) {
								return true;
							}
						}
						else {
							if (hitbox.box.top < offset.y + h ||
								hitbox.box.bottom > offset.y ||
								hitbox.box.left < offset.x + w ||
								hitbox.box.right > offset.x + partial.position) {
								return true;
							}
						}
					}
					break;
				}
				case Tile::Solidity::Slope:
				{
					const auto& slope = tile.solidity.slope;
					if (slope.above) {
						// TODO
						if (hitbox.box.bottom > offset.y ||
							hitbox.box.left < offset.x + w ||
							hitbox.box.right > offset.x) {
							return true;
						}
					}
					else {
						// TODO
						if (hitbox.box.top < offset.y + h ||
							hitbox.box.left < offset.x + w ||
							hitbox.box.right > offset.x) {
							return true;
						}
					}
				}
				case Tile::Solidity::Complex:
					if (hitboxes_overlap(
						hitbox, e->get_transform(), e->position - e->last_pos,
						tile.solidity.complex, Transform::translation(map.offset), { 0.f, 0.f }
					)) {
						return true;
					}
				default:
					break;
				}
			}
		}
	}
	return false;
}