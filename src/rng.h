#pragma once

#include <cstdint>
#include "angelscript.h"
#include "scriptarray/scriptarray.h"

#define CMWC_CYCLE 4096
#define CMWC_CARRY_MAX 809430660
#define CMWC_MULTIPLIER 18782
#define CMWC_MASK 0xfffffffe

/// Random Number Generator using CMWC4096
/// Heavyweight; ~16 KiB, only one RNG per thread.
/// Very long period
class Random {
private:
	uint32_t seq[CMWC_CYCLE];
	uint32_t carry;
	uint32_t ind;
public:
	Random(); /// seed from time
	Random(uint32_t seed); /// seed from single int

	Random(Random&&) = default;
	explicit Random(const Random&) = default; // Copies must be explicit

	void set_seed(uint32_t seed);

	uint32_t raw();
	uint64_t big();

	bool chance(float prob_true = 0.5f);

	// Uniformity is not guaranteed with int randoms, but it should be very close for small inputs

	int range(int low, int high, int step = 1); /// inclusive range
	int index(int size);
	int weighted(const CScriptArray& weights); /// weighted index
	inline int die(int sides) { return index(sides) + 1; }

	float standard(); /// 0 <= x < 1
	float interval(float low, float high); /// float in interval [low, high] | [low, high)
};

Random& get_thread_rng();

/// Random Number Generator using xorshift128
/// Suitable for creating local copies.
/// Lightweight, fast. (16 byte state, fewer instructions)
/// No overhead for acquiring rng for thread.
/// More deterministic when kept in local scope (would otherwise be affected by thread pool scheduling)
/// Constructable as value type by scripts.
class LightRandom {
private:
	uint32_t state[4];
public:
	LightRandom(); /// seed from time
	LightRandom(uint32_t seed); /// seed from single int

	LightRandom(LightRandom&&) = default;
	LightRandom(const LightRandom&) = default; // Copies are cheap

	~LightRandom() = default;

	uint32_t raw();
	uint64_t big();

	bool chance(float prob_true = 0.5f);

	// Uniformity is not guaranteed with int randoms, but it should be very close for small inputs

	int range(int low, int high, int step = 1); /// inclusive range
	int index(int size);
	int weighted(const CScriptArray& weights); /// weighted index
	inline int die(int sides) { return index(sides) + 1; }

	float standard(); /// 0 <= x < 1
	float interval(float low, float high); /// float in interval [low, high] | [low, high)
};

void RegisterRandomTypes(asIScriptEngine* engine);