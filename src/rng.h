#pragma once

#include <cstdint>
#include "angelscript.h"
#include "scriptarray/scriptarray.h"

#define CMWC_CYCLE 4096
#define CMWC_CARRY_MAX 809430660
#define CMWC_MULTIPLIER 18782
#define CMWC_MASK 0xfffffffe

/// Random Number Generator using CMWC4096
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


	static void RegisterType(asIScriptEngine* engine);
};