
#include "rng.h"
#include <ctime>
#include <cassert>
#include <cmath>
#include <thread>

// algorithms were lifted from Wikipedia. Yay!

Random::Random() {
	set_seed(time(nullptr));
}

Random::Random(uint32_t seed) {
	set_seed(seed);
}

void Random::set_seed(uint32_t seed) {
	ind = seed & (CMWC_CYCLE - 1);

	if (seed == 0) seed = 0xBEEF57E2; // just a fun easter egg since xorshift doesn't work with 0
	uint32_t x = seed;
	// populate initial state with inline xorshift32
	// I could just use the standard ANSI C rand function, but it's not implemented consistently
	// and I don't want to deal with patching implementation differences of RAND_MAX
	for (int i = 0; i < CMWC_CYCLE; ++i) {
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		seq[i] = x;
	}
	do { // This will be done, on average, about 5 times
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
	} while (x > CMWC_CARRY_MAX);
	carry = x;
}

uint32_t Random::raw() {
	constexpr uint64_t a = CMWC_MULTIPLIER;
	constexpr uint32_t mask = CMWC_MASK;

	ind = (ind + 1) & (CMWC_CYCLE - 1); // fast inc/mod
	uint64_t t = a * seq[ind] + carry;
	carry = t >> 32;
	uint32_t x = t + carry;
	if (x < carry) { // wraparound triggered
		++x;
		++carry;
	}
	return seq[ind] = mask - x;
}

uint64_t Random::big() {
	uint64_t v = raw();
	v = (v << 32) + raw();
	return v;
}

constexpr float intmax_float = (float)UINT_MAX + 1;

float Random::standard() {
	float r = (float) raw();
	return r / intmax_float;
}

float Random::interval(float low, float high) {
	float r = (float)raw();
	float std = r / intmax_float;
	float range = high - low;
	return std * range + low;
}

bool Random::chance(float prob_true) {
	return standard() < prob_true;
}

int Random::range(int low, int high, int step) {
	int size = (high - low + step) / step;
	return (raw() % size) * step + low;
}

int Random::index(int size) {
	return raw() % size;
}

int Random::weighted(const CScriptArray& weights) {
	assert(weights.GetElementTypeId() == asTYPEID_UINT32 && "weights were not uints");

	asUINT size = weights.GetSize();
	asUINT total_weight = 0;
	for (asUINT i = 0; i < size; ++i) {
		total_weight += *(asUINT*)(weights.At(i));
	}
	int x = raw() % total_weight;
	for (asUINT i = 0; i < size; ++i) {
		x -= *(asUINT*)(weights.At(i));
		if (x < 0) return i;
	}
	return size - 1;
}

LightRandom::LightRandom() : LightRandom(time(nullptr)) {}

LightRandom::LightRandom(uint32_t seed) {
	if (seed == 0) seed = 0xBEEF57E2; // just a fun easter egg since xorshift doesn't work with 0

	uint32_t x = seed;
	for (int i = 0; i < 4; ++i) {
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		state[i] = x;
	}
}

uint32_t LightRandom::raw() {
	uint32_t t = state[3];
	t ^= t << 11;
	t ^= t >> 8;
	state[3] = state[2]; state[2] = state[1]; state[1] = state[0];
	t ^= state[0];
	t ^= state[0] >> 19;
	state[0] = t;
	return t;
}
uint64_t LightRandom::big() {
	uint64_t v = raw();
	v = (v << 32) + raw();
	return v;
}

float LightRandom::standard() {
	float r = (float)raw();
	return r / intmax_float;
}

float LightRandom::interval(float low, float high) {
	float r = (float)raw();
	float std = r / intmax_float;
	float range = high - low;
	return std * range + low;
}

bool LightRandom::chance(float prob_true) {
	return standard() < prob_true;
}

int LightRandom::range(int low, int high, int step) {
	int size = (high - low + step) / step;
	return (raw() % size) * step + low;
}

int LightRandom::index(int size) {
	return raw() % size;
}

int LightRandom::weighted(const CScriptArray& weights) {
	assert(weights.GetElementTypeId() == asTYPEID_UINT32 && "weights were not uints");

	asUINT size = weights.GetSize();
	asUINT total_weight = 0;
	for (asUINT i = 0; i < size; ++i) {
		total_weight += *(asUINT*)(weights.At(i));
	}
	int x = raw() % total_weight;
	for (asUINT i = 0; i < size; ++i) {
		x -= *(asUINT*)(weights.At(i));
		if (x < 0) return i;
	}
	return size - 1;
}

void LightRandomConstructor(void* memory) {
	new(memory) LightRandom();
}

void LightRandomConstructor(void* memory, uint32_t seed) {
	new(memory) LightRandom(seed);
}

void LightRandomDestructor(void* memory, uint32_t seed) {
	((LightRandom*)memory)->~LightRandom();
}

void RegisterRandomTypes(asIScriptEngine* engine) {
	int r;

	r = engine->RegisterObjectType("__Random__", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

	r = engine->RegisterGlobalFunction("__Random__& get_rand()", asFUNCTION(get_thread_rng), asCALL_CDECL); assert(r >= 0);

	r = engine->RegisterObjectMethod("__Random__", "void seed(uint)",
		asMETHOD(Random, set_seed), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("__Random__", "uint raw()",
		asMETHOD(Random, raw), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("__Random__", "uint64 big()",
		asMETHOD(Random, big), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("__Random__", "float standard()",
		asMETHOD(Random, standard), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("__Random__", "float interval(float, float)",
		asMETHOD(Random, interval), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("__Random__", "bool chance(float)",
		asMETHOD(Random, chance), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("__Random__", "int range(int, int, int step = 1)",
		asMETHOD(Random, range), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("__Random__", "int index(int)",
		asMETHOD(Random, index), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("__Random__", "int weighted(const array<uint>&)",
		asMETHOD(Random, weighted), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("__Random__", "int die(int)",
		asMETHOD(Random, die), asCALL_THISCALL); assert(r >= 0);


	r = engine->RegisterObjectType("Random", sizeof(LightRandom),
		asOBJ_VALUE | asGetTypeTraits<LightRandom>()); assert(r >= 0);

	r = engine->RegisterObjectBehaviour("Random", asBEHAVE_CONSTRUCT, "void f()",
		asFUNCTIONPR(LightRandomConstructor, (void*), void), asCALL_CDECL_OBJFIRST); assert(r > 0);
	r = engine->RegisterObjectBehaviour("Random", asBEHAVE_CONSTRUCT, "void f(uint)",
		asFUNCTIONPR(LightRandomConstructor, (void*, uint32_t), void), asCALL_CDECL_OBJFIRST); assert(r > 0);
	r = engine->RegisterObjectBehaviour("Random", asBEHAVE_DESTRUCT, "void f()",
		asFUNCTION(LightRandomDestructor), asCALL_CDECL_OBJFIRST); assert(r > 0);

	r = engine->RegisterObjectMethod("Random", "uint raw()",
		asMETHOD(LightRandom, raw), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("Random", "uint64 big()",
		asMETHOD(LightRandom, big), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("Random", "float standard()",
		asMETHOD(LightRandom, standard), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("Random", "float interval(float, float)",
		asMETHOD(LightRandom, interval), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("Random", "bool chance(float)",
		asMETHOD(LightRandom, chance), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("Random", "int range(int, int, int step = 1)",
		asMETHOD(LightRandom, range), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("Random", "int index(int)",
		asMETHOD(LightRandom, index), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("Random", "int weighted(const array<uint>&)",
		asMETHOD(LightRandom, weighted), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("Random", "int die(int)",
		asMETHOD(LightRandom, die), asCALL_THISCALL); assert(r >= 0);
}

Random& get_thread_rng() {
	static std::hash<std::thread::id> hash;
	thread_local Random rng = Random(time(nullptr) ^ hash(std::this_thread::get_id()));
	return rng;
}