
#include "rng.h"
#include <ctime>
#include <cassert>
#include <cmath>

// algorithms were lifted from Wikipedia. Yay!

Random::Random() : Random(time(nullptr)) {}

Random::Random(uint32_t seed) {
	ind = seed & (CMWC_CYCLE - 1);

	if (seed == 0) seed = 0xBEEF57E2; // just a fun easter egg since xorshift doesn't work with 0
	uint32_t x = seed;
	// populate initial state with inline xorshift
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

void Random::RegisterType(asIScriptEngine* engine) {
	int r;

	r = engine->RegisterObjectType("Random", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

	r = engine->RegisterObjectMethod("Random", "uint raw()",
		asMETHOD(Random, raw), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("Random", "uint64 big()",
		asMETHOD(Random, big), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("Random", "float standard()",
		asMETHOD(Random, standard), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("Random", "float interval(float, float)",
		asMETHOD(Random, interval), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("Random", "bool chance(float)",
		asMETHOD(Random, chance), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("Random", "int range(int, int, int step = 1)",
		asMETHOD(Random, range), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("Random", "int index(int)",
		asMETHOD(Random, index), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("Random", "int weighted(const array<uint>&)",
		asMETHOD(Random, weighted), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("Random", "int die(int)",
		asMETHOD(Random, die), asCALL_THISCALL); assert(r >= 0);
}