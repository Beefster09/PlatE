
#include "level.h"
#include "storage.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"

using namespace rapidjson;

Either<Error, Level*> load_level_json(const char* filename) {
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

	MemoryPool pool(65536); // Size is temporary
}