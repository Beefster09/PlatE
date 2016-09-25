
#include "storage.h"
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

	// TODO: Determine size needed for everything

	auto pool = MemoryPool(65536); // size is temporary
	Sprite* result = pool.alloc<Sprite>();

	// Clips
	auto clipJson = json["clips"].GetArray();

	int n_clipRects = clipJson.Size();

	SDL_Rect* clipRects = pool.alloc<SDL_Rect>(n_clipRects);

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

	Frame* frames = pool.alloc<Frame>(n_frames);

	for (int i = 0; i < n_frames; ++i) {
		auto frameJson = framesJson[i].GetObject();

		// Hitbox Groups
		auto collisionJson = frameJson["collision"].GetArray();
		int n_hitboxGroups = collisionJson.Size();
		HitboxGroup* collision = pool.alloc<HitboxGroup>(n_hitboxGroups);

		for (int j = 0; j < n_hitboxGroups; ++j) {
			auto hitboxGroupJson = collisionJson[j].GetObject();

			// Hitboxes
			auto hitboxesJson = hitboxGroupJson["hitboxes"].GetArray();
			int n_hitboxes = hitboxesJson.Size();
			Hitbox* hitboxes = pool.alloc<Hitbox>(n_hitboxes);

			for (int k = 0; k < n_hitboxes; ++k) {
				auto hitboxJson = hitboxesJson[k].GetObject();
				// TODO: Circle, Line collision
				hitboxes[k] = {
					Hitbox::BOX,
					hitboxJson["x"].GetInt(),
					hitboxJson["y"].GetInt(),
					hitboxJson["w"].GetInt(),
					hitboxJson["h"].GetInt()
				};
			}

			collision[j] = {
				hitbox_type_by_name(hitboxGroupJson["type"].GetString()),
				{},
				(uint64_t)hitboxGroupJson["flags"].GetInt64(),
				Array<const Hitbox>(hitboxes, n_hitboxes)
			};
			// TODO: special data
		}

		frames[i] = {
			&clipRects[frameJson["clip"].GetInt()],
			frame_offset_json(frameJson["display"]),
			frame_offset_json(frameJson["foot"]),
			Array<const FrameOffset>(nullptr, 0),
			CollisionData{
				0.f,
				Array<const HitboxGroup>(collision, n_hitboxGroups)
			}
		};
	}

	// Animations

	auto animationsJson = json["animations"].GetArray();
	int n_animations = animationsJson.Size();
	Animation* animations = pool.alloc<Animation>(n_animations);

	for (int i = 0; i < n_animations; ++i) {
		auto animationJson = animationsJson[i].GetObject();

		auto sequenceJson = animationJson["frames"].GetArray();
		int n_entries = sequenceJson.Size();
		FrameTiming* sequence = pool.alloc<FrameTiming>(n_entries);

		for (int j = 0; j < n_entries; ++j) {
			auto timingJson = sequenceJson[j].GetObject();
			sequence[j] = {
				timingJson["duration"].GetFloat(),
				&frames[timingJson["frame"].GetInt()]
			};
		}

		animations[i] = {
			pool.alloc_str(animationJson["name"].GetString()),
			Array<const FrameTiming>(sequence, n_entries)
		};
	}

	// Now to put it all together!

	*result = {
		pool.alloc_str(json["name"].GetString()),
		nullptr,
		Array<const SDL_Rect>(clipRects, n_clipRects),
		Array<const Frame>(frames, n_frames),
		Array<const Animation>(animations, n_animations)
	};

	// json is RAII; no need to free
	// return sprite pointer
	return result;
}