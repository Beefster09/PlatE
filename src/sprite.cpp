
#include "mempool.h"
#include "sprite.h"
#include "error.h"
#include "fileutil.h"
#include "assetmanager.h"
#include <cstdio>
#include <cerrno>
#include <cstring>

__forceinline static Result<const Sprite*> read_sprite(FILE* stream, MemoryPool& pool,
	uint32_t namelen, uint32_t texnamelen, uint32_t n_clips, uint32_t n_frames, uint32_t n_animations,
	const DirContext& context);

Result<const Sprite*> load_sprite(const char* filename, const DirContext& context) {
	std::string realfile;
	check_assign(realfile, context.resolve(filename));

	{
		const Sprite* maybe = AssetManager::retrieve<Sprite>(realfile.c_str());
		if (maybe != nullptr) return maybe;
	}

	auto file = open(realfile.c_str(), "rb");

	if (!file) {
		return file.err;
	}
	FILE* stream = file;

	// Check the magic number
	if (!check_header(stream, SPRITE_MAGIC_NUMBER)) {
		return Errors::InvalidSpriteHeader;
	}

	// Read the header to determine how much we need to allocate in the memory pool
	uint32_t namelen, texnamelen, n_clips, n_frames, n_animations,
		tn_offsets, tn_colliders, nested_hitboxes,
		tn_vertices, tn_frametimings, tn_stringbytes;

	try {
		namelen = read<uint32_t>(stream);
		texnamelen = read<uint32_t>(stream);
		n_clips = read<uint32_t>(stream);
		n_frames = read<uint32_t>(stream);
		n_animations = read<uint32_t>(stream);
		tn_offsets = read<uint32_t>(stream);
		tn_colliders = read<uint32_t>(stream);
		nested_hitboxes = read<uint32_t>(stream);
		tn_vertices = read<uint32_t>(stream);
		tn_frametimings = read<uint32_t>(stream);
		tn_stringbytes = read<uint32_t>(stream);
	}
	catch (Error& err) {
		fclose(stream);
		return err;
	}

	size_t poolsize =
		sizeof(Sprite) +
		n_clips * sizeof(SDL_Rect) +
		n_frames * sizeof(Frame) +
		n_animations * sizeof(Animation) +
		(tn_vertices + tn_offsets) * sizeof(Vector2) +
		tn_colliders * sizeof(Collider) +
		nested_hitboxes * sizeof(Hitbox) +
		tn_frametimings * sizeof(FrameTiming) +
		tn_stringbytes;

	LOG_VERBOSE("Number of bytes needed for sprite data: %zd\n", poolsize);
	MemoryPool pool(poolsize);

	check_assign_ref(const DirContext& subcontext, context + filename, sctx);
	// Pulled out into an inline function to ensure we never leak the file
	auto result = read_sprite(stream, pool,
		namelen, texnamelen, n_clips, n_frames, n_animations,
		subcontext);

	LOG_VERBOSE("Read sprite data with %zd/%zd bytes of slack in memory pool\n", pool.get_slack(), pool.get_size());

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

__forceinline static Result<const Sprite*> read_sprite(FILE* stream, MemoryPool& pool,
	uint32_t namelen, uint32_t texnamelen, uint32_t n_clips, uint32_t n_frames, uint32_t n_animations,
	const DirContext& context) {
	try {
		Sprite* sprite = pool.alloc<Sprite>();

		sprite->name = read_string(stream, namelen, pool);

		sprite->texture = read_referenced_texture(stream, texnamelen, context);

		GPU_Rect* clips = pool.alloc<GPU_Rect>(n_clips);
		for (int i = 0; i < n_clips; ++i) {
			clips[i].x = (float) read<uint32_t>(stream);
			clips[i].y = (float) read<uint32_t>(stream);
			clips[i].w = (float) read<uint32_t>(stream);
			clips[i].h = (float) read<uint32_t>(stream);
		}

		Frame* frames = pool.alloc<Frame>(n_frames);
		for (int i = 0; i < n_frames; ++i) {
			Frame& cur_frame = frames[i];
			cur_frame.clip = &clips[read<uint32_t>(stream)];
			cur_frame.display = read<Vector2>(stream);

			size_t n_offsets = read<uint32_t>(stream);
			size_t n_colliders = read<uint32_t>(stream);

			Vector2* offsets = pool.alloc<Vector2>(n_offsets);
			for (int j = 0; j < n_offsets; ++j) {
				offsets[j] = read<Vector2>(stream);
			}
			new(&cur_frame.offsets) Array<const Vector2>(offsets, n_offsets);

			cur_frame.colliders = read_colliders(stream, n_colliders, pool);
		}

		Animation* animations = pool.alloc<Animation>(n_animations);

		for (int i = 0; i < n_animations; ++i) {
			Animation& cur_anim = animations[i];

			size_t namelen = read<uint32_t>(stream);
			size_t n_timings = read<uint32_t>(stream);
			cur_anim.solidity.fixed = read<bool>(stream);

			cur_anim.name = read_string(stream, namelen, pool);
			cur_anim.solidity.hitbox = read_hitbox(stream, pool);

			// precalculate head and foot y coordinates (x is always 0)
			switch (cur_anim.solidity.hitbox.type) {
				// this is all a bit noisy in source, but the offsets should get optimized quite well by the compiler
			case Hitbox::BOX:
				cur_anim.solidity.head = cur_anim.solidity.hitbox.box.top;
				cur_anim.solidity.foot = cur_anim.solidity.hitbox.box.bottom;
				break;
			case Hitbox::CIRCLE:
				cur_anim.solidity.head = cur_anim.solidity.hitbox.circle.center.y - cur_anim.solidity.hitbox.circle.radius;
				cur_anim.solidity.foot = cur_anim.solidity.hitbox.circle.center.y + cur_anim.solidity.hitbox.circle.radius;
				break;
			case Hitbox::LINE:
			case Hitbox::ONEWAY:
				cur_anim.solidity.head = SDL_min(cur_anim.solidity.hitbox.line.p1.y, cur_anim.solidity.hitbox.line.p2.y);
				cur_anim.solidity.foot = SDL_max(cur_anim.solidity.hitbox.line.p1.y, cur_anim.solidity.hitbox.line.p2.y);
				break;
			case Hitbox::POLYGON:
				cur_anim.solidity.head = cur_anim.solidity.hitbox.polygon.aabb.top;
				cur_anim.solidity.foot = cur_anim.solidity.hitbox.polygon.aabb.bottom;
				break;
			case Hitbox::COMPOSITE:
				cur_anim.solidity.head = cur_anim.solidity.hitbox.composite.aabb.top;
				cur_anim.solidity.foot = cur_anim.solidity.hitbox.composite.aabb.bottom;
				break;
			default:
				cur_anim.solidity.head = 0.f;
				cur_anim.solidity.foot = 0.f;
				break;
			}

			FrameTiming* timings = pool.alloc<FrameTiming>(n_timings);
			for (int j = 0; j < n_timings; ++j) {
				FrameTiming& timing = timings[j];

				timing.delay = read<float>(stream);
				timing.frame = &frames[read<uint32_t>(stream)];
			}

			new(&cur_anim.frames) Array<const FrameTiming>(timings, n_timings);
		}

		new(&sprite->clips) Array<const GPU_Rect>(clips, n_clips);
		new(&sprite->framedata) Array<const Frame>(frames, n_frames);
		new(&sprite->animations) Array<const Animation>(animations, n_animations);

		return sprite;
	}
	catch (Error& err) {
		return err;
	}
}

Result<const Sprite*> read_referenced_sprite(FILE* stream, uint32_t len, const DirContext& context) {
	char fn[1024];
	fread(fn, 1, len, stream);
	fn[len] = 0;

	return load_sprite(fn, context);
}