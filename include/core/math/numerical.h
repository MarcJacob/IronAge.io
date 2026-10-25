// Simple math functions applied over scalars / general mathematical values.

#ifndef NUMERICAL_INCLUDED
#define NUMERICAL_INCLUDED

#define ia_min(a, b) ((a) <= (b) ? (a) : (b))
#define ia_max(a, b) ((a) >= (b) ? (a) : (b))

template<typename Numeric>
static Numeric ia_pow(Numeric base_val, ui32 pow)
{
	if (pow == 0) return 1;
	if (base_val == 0) return 0;

	Numeric val = base_val;
	for (ui32 i = 1; i < pow; i++)
	{
		val = val * base_val;
	}

	return val;
}

// Handmade square root function for when libc isn't available.
// NOTE(Marc): This could be optimized quite a bit I imagine, and there's probably some intrinsic I'm missing. I heard compilers
// pretty much have the square root operation baked in these days.
// precision_level determines the max amount of binary search steps for finding the precise square root value. Default value is good enough for most usage.
template<typename Numeric>
static float ia_sqrt(Numeric val, ui8 precision_level = 8)
{
	if (val == 0) return 0;
	if (val == 1) return 1;
	if (val == 2) return 1.4142f;

	ASSERT(val > 2);

	// Determine max possible value in base 2 then go downwards in a binary search fashion until reaching an appropriate precision level.
	// NOTE(Marc): At least that's I've just come up with. I've actually never implemented my own sqrt function before, believe it or not.

	ui64 intVal = (ui64)val;
	ui8 mostSignificantBit = 1; // Since we know value is at least 3.

	while (intVal >= (1 << (mostSignificantBit + 1)))
	{
		mostSignificantBit++;
	}

	// Maximum possible value for the square root is log2(value) / 2, rounded up to the higher bit if needed (odd most signiticant bit index).
	float maxResult = 1 << (mostSignificantBit / 2 + mostSignificantBit % 2);

	// And we know that it can't be smaller than one bit lower than log2(value) / 2.
	float minResult = (1 << (mostSignificantBit / 2 - (1 - mostSignificantBit % 2)));

	// Easy case: value is exactly equal to the most significant bit.
	if (intVal == (1 << mostSignificantBit)) return maxResult;

	// Now for the hard part. We know the range in which the root is located, 
	// now we reduce that range to a smaller and smaller magnitude.

	// TEMP(Marc): Let's see what happens with a static precision level. We won't be calling this very often and preferably not on super large numbers.
	for (ui8 precisionLevel = 0; precisionLevel < precision_level; precisionLevel++)
	{
		float estimate = (minResult + maxResult) / 2.f;
		float estimatePow = ia_pow(estimate, 2);
		if (estimatePow > val) maxResult = estimate;
		else if (estimatePow < val) minResult = estimate;
		else return estimate; // NOTE(Marc): Not sure the extra branch for this early out is really worthwhile unless we use a very high precision level...
	}

	return minResult; // Arbitrarily return lower bound of range.
}

template<typename Numeric>
static inline Numeric ia_abs(Numeric val)
{
	return val - (2 * val * (val < 0));
}

// Returns value clamped between min and max, both INCLUDED as possible values (IE ia_clamped(1, 0, 1) -> 1).
template<typename Numeric>
static inline Numeric ia_clamped(Numeric val, Numeric min, Numeric max)
{
    return val < min ? min : (val > max ? max : val);
}

// FIXED-POINT HELPERS

// Integer square root: the largest r such that r * r <= val. Exact and integer-only, so results are identical on every machine.
static inline ui32 ia_isqrt(ui64 val)
{
	ui64 result = 0;
	ui64 bit = (ui64)1 << 62;
	while (bit > val) bit >>= 2;

	while (bit != 0)
	{
		if (val >= result + bit)
		{
			val -= result + bit;
			result = (result >> 1) + bit;
		}
		else result >>= 1;
		bit >>= 2;
	}
	return (ui32)result;
}

// Approximate log2(n) in fixed point with 8 fractional bits (256 = 1.0). n = 0 returns 0.
// Piecewise linear between powers of two: exact at powers of two, monotonic non-decreasing.
static inline ui32 ia_log2_q8(ui64 val)
{
	if (val <= 1) return 0;

	ui32 mostSignificantBit = 0;
	while (mostSignificantBit < 63 && (val >> (mostSignificantBit + 1)) != 0) mostSignificantBit++;

	ui64 aboveLowerPowerOfTwo = val - ((ui64)1 << mostSignificantBit); // < 2^mostSignificantBit
	ui32 fractionQ8 = (ui32)(mostSignificantBit >= 8 ? aboveLowerPowerOfTwo >> (mostSignificantBit - 8) : aboveLowerPowerOfTwo << (8 - mostSignificantBit)); // < 256
	return mostSignificantBit * 256 + fractionQ8;
}

// Fractions are ui32 values in [0, IA_FIXED_ONE] with 16 fractional bits. Integer-only, so results are identical on every machine.
static constexpr ui32 IA_FIXED_ONE = 1 << 16;

// Linear interpolation from a to b, with t a fixed-point fraction (0 = a, IA_FIXED_ONE = b).
static inline i32 ia_lerp_fixed(i32 a, i32 b, ui32 t)
{
	return a + (i32)(((i64)(b - a) * (i64)t) / (i64)IA_FIXED_ONE);
}

// Smoothstep curve (3t^2 - 2t^3) of a fixed-point fraction in [0, IA_FIXED_ONE]. Returns a fixed-point fraction.
static inline ui32 ia_smoothstep_fixed(ui32 t)
{
	ui64 t2 = ((ui64)t * t) / IA_FIXED_ONE;
	return (ui32)((t2 * (3ull * IA_FIXED_ONE - 2ull * t)) / IA_FIXED_ONE);
}

#endif // NUMERICAL_INCLUDED

