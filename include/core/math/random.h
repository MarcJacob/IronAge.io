// Random number generation tools.

#ifndef RANDOM_INCLUDED
#define RANDOM_INCLUDED

#include "../std_types.h"
#include "../assert.h"

#include "vec2.h"
#include "numerical.h"

// Default state value for the stateful ia_rand_XX generators.
// Gets mutated on each call to ia_rand_XX unless they use their own state value.
static ui32 XORSHIFT_STATIC_STATE_32 = 0x9E3779B9;
static ui64 XORSHIFT_STATIC_STATE_64 = 0x7A10FCC7435E5FC2;

// Simple xorshift32 PRNG.
// Uses an externally passed state value or the default statically-stored one.
// The state gets mutated to the returned value.
static inline ui32 ia_xorshift_i32(ui32& state = ::XORSHIFT_STATIC_STATE_32)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

static inline ui64 ia_xorshift_i64(ui64& state = ::XORSHIFT_STATIC_STATE_64)
{
	state ^= state << 7;
	state ^= state >> 9;
	return state;
}

// XORSHIFT-based ranged value generator using float math.
template<typename Numeric>
static inline Numeric ia_xorshift_range_f32(Numeric min, Numeric max, ui32& state = ::XORSHIFT_STATIC_STATE_32)
{
	float alpha = ia_xorshift_i32(state) / (float)0xFFFFFFFFu;
	return min + (max - min) * alpha;
}

// XORSHIFT-based ranged value generator using double math.
template<typename Numeric>
static inline Numeric ia_xorshift_range_f64(Numeric min, Numeric max, ui64& state = ::XORSHIFT_STATIC_STATE_64)
{
	double alpha = ia_xorshift_i64(state) / (double)0xFFFFFFFFFFFFFFFFu;
	return min + (max - min) * alpha;
}

// XORSHIFT-based ranged value generator using 32 bits integer math.
// The range length (max - min) must be at least 1.
// TODO(Marc): Support any step size, which in turn would lift the range length constraint.
template<typename Numeric>
static inline Numeric ia_xorshift_range_i32(Numeric min, Numeric max, ui32& state = ::XORSHIFT_STATIC_STATE_32)
{
	ASSERT(max - min >= 1);

	ui32 quanta = ia_xorshift_i32(state); // Number of quanta.
	ui32 quanta_per_step = 0xFFFFFFFFu / (Numeric)(max - min); // Number of quanta between each step from min to max.
	
	return min + (Numeric)(quanta / quanta_per_step);
}

template<>
float ia_xorshift_range_i32<float>(float min, float max, ui32& state)
{
	ASSERT(max - min >= 1);

	ui32 quanta = ia_xorshift_i32(state); // Number of quanta.
	float quanta_per_step = (float)0xFFFFFFFFu / (float)(max - min); // Number of quanta between each step from min to max.
	
	return min + (quanta / quanta_per_step);

}

// Stateful 32-bits xorshift-based generator.
// All generation within is based on i32 math for better determinism across machines.
struct rand_generator_32
{
	ui32 _state; // State bytes used by the next_XX functions.

	template<typename Numeric>
	inline Numeric next() {
		static_assert(sizeof(Numeric) <= 4, "Cannot generate a random value of larger size than 4 bytes with this generator.");
		return (Numeric)ia_xorshift_i32(_state);
	}

	// Generates a random value of the type in provided range.
	// Accepts any value of size 64 or lower (with extra zeroes
	template<typename Numeric>
	inline Numeric next_range(Numeric min, Numeric max) {
		static_assert(sizeof(Numeric) <= 4, "Cannot generate a random value of larger size than 4 bytes with this generator.");
		return ia_xorshift_range_i32<Numeric>(min, max, _state);
	}

	// Generates random vector from min to max.
	template<typename Numeric>
	inline vec2<Numeric> next_vec2(vec2<Numeric> min, vec2<Numeric> max)
	{
		static_assert(sizeof(Numeric) <= 4, "Cannot generate a random value of larger size than 4 bytes with this generator.");
		vec2<Numeric> rand = {
			ia_xorshift_range_i32<Numeric>(min.x, max.x, _state),
			ia_xorshift_range_i32<Numeric>(min.y, max.y, _state),
		};

		return rand;
	}

	// Generates random vector from (0, 0) to max.
	template<typename Numeric>
	inline vec2<Numeric> next_vec2(vec2<Numeric> max)
	{
		static_assert(sizeof(Numeric) <= 4, "Cannot generate a random value of larger size than 4 bytes with this generator.");
		vec2<Numeric> rand = {
			(Numeric)ia_xorshift_range_i32(0.f, (float)max.x, _state),
			(Numeric)ia_xorshift_range_i32(0.f, (float)max.y, _state),
		};

		return rand;
	}
};

// Creates a new random generator with the given seed value.
static inline rand_generator_32 rand_generator_create_32(ui32 seed)
{
	return { seed };
}

// Creates a new random generator by "spawning" if from an existing one (asking it to generate the seed).
static inline rand_generator_32 rand_generator_create_32(rand_generator_32& spawner)
{
	ui32 seed = spawner.next<ui32>();
	return { seed };
}

#endif // RANDOM_INCLUDED