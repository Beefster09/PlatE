
#include "sprite.h"
#include "error.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"
#include <cstdio>
#include <cerrno>

using namespace rapidjson;

static Either<const char*, bool> validate_sprite_json(Document& json) {
	if (!json.IsObject()) return "/ is not an object";
	if (!json["name"].IsString()) return "/name is not a string";
	if (!json["spritesheet"].IsString()) return "/spritesheet is not a string";
	if (!json["clips"].IsArray()) return "/clips is not an array";
	if (!json["frames"].IsArray()) return "/frames is not an array";
	if (!json["animations"].IsArray()) return "/animations is not an array";

	return true;
}

static FrameOffset frame_offset_json(Value& json) {
	auto offsetJson = json.GetObject();
	return{
		(short)offsetJson["x"].GetInt(),
		(short)offsetJson["y"].GetInt()
	};
}

template<class T>
static T* pool_alloc(void* pool, size_t& offset, size_t quantity) {
	T* pointer = (T*)pool + offset;
	offset += quantity * sizeof(T);
	return pointer;
}

const Either<Error, const Sprite*> load_sprite_json(char* filename) {
	Document json;

	// Parse to json
	{
		FILE* fp = fopen(filename, "rb");
		if (fp == 0) {
			return Errors::FileNotFound;
		}
		char readbuffer[65536];
		// Streaming JSON just in case the JSON is somehow larger than 64 KiB...
		FileReadStream stream(fp, readbuffer, 65536);
		json.ParseStream(stream);
		fclose(fp);
	}

	if (json.HasParseError()) {
		const char* message = GetParseError_En(json.GetParseError());
		return Errors::InvalidJson;
	}

	auto validation = validate_sprite_json(json);
	if (validation.isLeft) {
		return DetailedError(Errors::SpriteLoadJsonInvalid, "%s\n", validation.left);
	}

	// TODO: Determine size needed for everything (use pool allocation)

	// WARNING: SERIOUS POINTER-FU AHEAD
	void* pool = malloc(65536); // TEMP
	size_t nextAllocOffset = sizeof(Sprite);

	auto allocString = [pool, &nextAllocOffset](const char* str) mutable -> char * {
		char* result = (char*)pool + nextAllocOffset;
		strcpy(result, str);
		nextAllocOffset += strlen(str) + 1;
		return result;
	};

	// Clips
	auto clipJson = json["clips"].GetArray();

	int n_clipRects = clipJson.Size();

	SDL_Rect* clipRects = pool_alloc<SDL_Rect>(pool, nextAllocOffset, n_clipRects);

	for (int i = 0; i < n_clipRects; ++i) {
		auto rectJson = clipJson[i].GetObject();
		clipRects[i] = {
			rectJson["x"].GetInt(),
			rectJson["y"].GetInt(),
			rectJson["w"].GetInt(),
			rectJson["h"].GetInt()
		};
	}

	// Frames
	auto framesJson = json["frames"].GetArray();

	int n_frames = framesJson.Size();

	Frame* frames = pool_alloc<Frame>(pool, nextAllocOffset, n_frames);

	for (int i = 0; i < n_frames; ++i) {
		auto frameJson = framesJson[i].GetObject();

		// Hitbox Groups
		auto collisionJson = frameJson["collision"].GetArray();
		int n_hitboxGroups = collisionJson.Size();
		HitboxGroup* collision = pool_alloc<HitboxGroup>(pool, nextAllocOffset, n_hitboxGroups);

		for (int j = 0; j < n_hitboxGroups; ++j) {
			auto hitboxGroupJson = collisionJson[j].GetObject();

			// Hitboxes
			auto hitboxesJson = hitboxGroupJson["hitboxes"].GetArray();
			int n_hitboxes = hitboxesJson.Size();
			Hitbox* hitboxes = pool_alloc<Hitbox>(pool, nextAllocOffset, n_hitboxes);

			for (int k = 0; k < n_hitboxes; ++k) {
				auto hitboxJson = hitboxesJson[k].GetObject();
				// TODO: Circle, Line collision
				hitboxes[k] = {
					HitboxShape::BOX,
					(short) hitboxJson["x"].GetInt(),
					(short) hitboxJson["y"].GetInt(),
					(short) hitboxJson["w"].GetInt(),
					(short) hitboxJson["h"].GetInt()
				};
			}

			collision[j] = {
				hitbox_type_by_name(hitboxGroupJson["type"].GetString()),
				n_hitboxes,
				hitboxes,
				(uint64_t)hitboxGroupJson["flags"].GetInt64(),
				0
			};
			// TODO: special data
		}

		frames[i] = {
			&clipRects[frameJson["clip"].GetInt()],
			frame_offset_json(frameJson["display"]),
			frame_offset_json(frameJson["foot"]),
			0, // TODO: alt offsets
			nullptr,
			n_hitboxGroups,
			collision
		};
	}

	// Animations

	auto animationsJson = json["animations"].GetArray();
	int n_animations = animationsJson.Size();
	Animation* animations = pool_alloc<Animation>(pool, nextAllocOffset, n_animations);

	for (int i = 0; i < n_animations; ++i) {
		auto animationJson = animationsJson[i].GetObject();

		auto sequenceJson = animationJson["frames"].GetArray();
		int n_entries = sequenceJson.Size();
		FrameTiming* sequence = pool_alloc<FrameTiming>(pool, nextAllocOffset, n_entries);

		for (int j = 0; j < n_entries; ++j) {
			auto timingJson = sequenceJson[j].GetObject();
			sequence[j] = {
				timingJson["duration"].GetFloat(),
				&frames[timingJson["frame"].GetInt()]
			};
		}

		animations[i] = {
			allocString(animationJson["name"].GetString()),
			n_entries,
			sequence
		};
	}

	// Now to put it all together!

	*(Sprite*)pool = {
		allocString(json["name"].GetString()),
		nullptr,
		n_clipRects,
		clipRects,
		n_frames,
		frames,
		n_animations,
		animations
	};

	// json is RAII; no need to free
	// return sprite pointer
	return (Sprite*)pool;
}