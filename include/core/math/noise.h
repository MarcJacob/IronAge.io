
// Integer-only hashing and value noise, for deterministic procedural generation across machines.

#ifndef NOISE_INCLUDED
#define NOISE_INCLUDED

#include "../std_types.h"
#include "../assert.h"
#include "numerical.h"

// Integer hash with good avalanche (lowbias32). Same input always gives the same output on every machine.
static inline ui32 ia_hash_ui32(ui32 x)
{
	x ^= x >> 16;
	x *= 0x7FEB352Du;
	x ^= x >> 15;
	x *= 0x846CA68Bu;
	x ^= x >> 16;
	return x;
}

// Hash of a 2D integer position and a seed.
static inline ui32 ia_hash_2d(ui32 x, ui32 y, ui32 seed)
{
	return ia_hash_ui32(x ^ ia_hash_ui32(y ^ ia_hash_ui32(seed)));
}

// Smooth value noise at integer position (x, y). Random values sit on a lattice of cell_size units (must be at least 1)
// and are interpolated between with a smoothstep. Returns a value in [0, 65535].
static inline ui16 ia_value_noise_2d(ui32 x, ui32 y, ui32 cell_size, ui32 seed)
{
	ASSERT(cell_size >= 1);

	ui32 cellX = x / cell_size;
	ui32 cellY = y / cell_size;
	ui32 fracX = ia_smoothstep_fixed((ui32)((ui64)(x % cell_size) * IA_FIXED_ONE / cell_size));
	ui32 fracY = ia_smoothstep_fixed((ui32)((ui64)(y % cell_size) * IA_FIXED_ONE / cell_size));

	i32 v00 = (i32)(ia_hash_2d(cellX, cellY, seed) >> 16);
	i32 v10 = (i32)(ia_hash_2d(cellX + 1, cellY, seed) >> 16);
	i32 v01 = (i32)(ia_hash_2d(cellX, cellY + 1, seed) >> 16);
	i32 v11 = (i32)(ia_hash_2d(cellX + 1, cellY + 1, seed) >> 16);

	return (ui16)ia_lerp_fixed(ia_lerp_fixed(v00, v10, fracX), ia_lerp_fixed(v01, v11, fracX), fracY);
}

// Sum of value noise octaves: the first one has the given cell size, each next one has half the cell size (min 1) and half the weight.
// octaves must be between 1 and 16. Returns a value in [0, 65535].
static inline ui16 ia_fractal_noise_2d(ui32 x, ui32 y, ui32 cell_size, ui8 octaves, ui32 seed)
{
	ASSERT(octaves >= 1 && octaves <= 16);

	ui64 sum = 0;
	ui64 weightSum = 0;
	for (ui8 octave = 0; octave < octaves; octave++)
	{
		ui32 weight = 1u << (octaves - 1 - octave);
		sum += (ui64)ia_value_noise_2d(x, y, cell_size, seed + octave) * weight;
		weightSum += weight;
		cell_size = ia_max(cell_size / 2, 1u);
	}
	return (ui16)(sum / weightSum);
}

#endif // NOISE_INCLUDED
