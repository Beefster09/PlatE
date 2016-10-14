
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

static Vector2 vector_json(Value& json) {
	auto offsetJson = json.GetObject();
	return{
		offsetJson["x"].GetFloat(),
		offsetJson["y"].GetFloat()
	};
}

static Hitbox hitbox_json(Value& val, MemoryPool& pool) {
	Hitbox result;
	auto json = val.GetObject();
	const char* type = json["type"].GetString();

	if (strcmp(type, "box") == 0) {
		result.type = Hitbox::BOX;
		result.box = {
			json["left"].GetFloat(),
			json["right"].GetFloat(),
			json["top"].GetFloat(),
			json["bottom"].GetFloat()
		};
	}
	else if (strcmp(type, "circle") == 0) {
		result.type = Hitbox::CIRCLE;
		result.circle = {
			vector_json(json["center"]),
			json["radius"].GetFloat()
		};
	}
	else if (strcmp(type, "line") == 0) {
		result.type = Hitbox::LINE;
		result.line = {
			vector_json(json["p1"]),
			vector_json(json["p2"])
		};
	}
	else if (strcmp(type, "oneway") == 0) {
		result.type = Hitbox::ONEWAY;
		result.line = {
			vector_json(json["p1"]),
			vector_json(json["p2"])
		};
	}
	else if (strcmp(type, "polygon") == 0) {
		result.type = Hitbox::POLYGON;
		// TODO
	}
	else if (strcmp(type, "composite") == 0) {
		result.type = Hitbox::COMPOSITE;
		auto subs_json = json["hitboxes"].GetArray();
		int n_subs = subs_json.Size();
		Hitbox* subHits = pool.alloc<Hitbox>(n_subs);
		for (int i = 0; i < n_subs; ++i) {
			subHits[i] = hitbox_json(subs_json[i], pool);
		}
		// TODO: generate AABB
	}
	else {
		//return Error
	}
	return result;
}

static bool bool_json(Value& json) {
	return json.IsBool() && json.IsTrue();
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

	MemoryPool pool(65536); // size is temporary
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

		// colliders
		auto collisionJson = frameJson["collision"].GetArray();
		int n_colliders = collisionJson.Size();
		Collider* collision = pool.alloc<Collider>(n_colliders);

		for (int j = 0; j < n_colliders; ++j) {
			auto colliderJson = collisionJson[j].GetObject();

			collision[j] = {
				CollisionType::by_name(colliderJson["type"].GetString()),
				hitbox_json(colliderJson["hitbox"], pool),
				bool_json(colliderJson["solid"]),
				bool_json(colliderJson["ccd"])
			};
		}

		frames[i] = {
			&clipRects[frameJson["clip"].GetInt()],
			vector_json(frameJson["display"]),
			vector_json(frameJson["foot"]),
			Array<const FrameOffset>(nullptr, 0),
			Array<const Collider>(collision, n_colliders)
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