
#include "sprite.h"
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
			return ERROR(ERROR_LOADSPRITE_NOTFOUND, strerror(errno));
		}
		char readbuffer[65536];
		// Streaming JSON just in case the JSON is somehow larger than 64 KiB...
		FileReadStream stream(fp, readbuffer, 65536);
		json.ParseStream(stream);
		fclose(fp);
	}

	if (json.HasParseError()) {
		const char* message = GetParseError_En(json.GetParseError());
		return ERROR(ERROR_LOADSPRITE_JSONPARSE, message);
	}

	auto validation = validate_sprite_json(json);
	if (validation.isLeft) {
		return ERROR(ERROR_LOADSPRITE_JSONVALIDATION, validation.left);
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

	SDL_Rect* clipRects = (SDL_Rect*)pool + nextAllocOffset;
	nextAllocOffset += n_clipRects * sizeof(SDL_Rect);

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

	Frame* frames = (Frame*)pool + nextAllocOffset;
	nextAllocOffset += n_frames * sizeof(Frame);

	for (int i = 0; i < n_frames; ++i) {
		auto frameJson = framesJson[i].GetObject();

		// Hitbox Groups
		auto collisionJson = frameJson["collision"].GetArray();
		int n_hitboxGroups = collisionJson.Size();
		HitboxGroup* collision = (HitboxGroup*)pool + nextAllocOffset;
		nextAllocOffset += n_hitboxGroups * sizeof(HitboxGroup);

		for (int j = 0; j < n_hitboxGroups; ++j) {
			auto hitboxGroupJson = collisionJson[j].GetObject();

			// Hitboxes
			auto hitboxesJson = hitboxGroupJson["hitboxes"].GetArray();
			int n_hitboxes = hitboxesJson.Size();
			Hitbox* hitboxes = (Hitbox*)pool + nextAllocOffset;
			nextAllocOffset += n_hitboxes * sizeof(Hitbox);

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
				hitbox_type_from_string(hitboxGroupJson["type"].GetString()),
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
	Animation* animations = (Animation*)pool + nextAllocOffset;
	nextAllocOffset += n_animations * sizeof(Animation);

	for (int i = 0; i < n_animations; ++i) {
		auto animationJson = animationsJson[i].GetObject();

		auto sequenceJson = animationJson["frames"].GetArray();
		int n_entries = sequenceJson.Size();
		FrameTiming* sequence = (FrameTiming*)pool + nextAllocOffset;
		nextAllocOffset += n_entries * sizeof(FrameTiming);

		// PROBLEM IS IN THIS FOR LOOP
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