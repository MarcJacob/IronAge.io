// Simple math functions applied over scalars / general mathematical values.

#ifndef NUMERICAL_INCLUDED
#define NUMERICAL_INCLUDED

#define ia_min(a, b) (a <= b ? a : b)
#define ia_max(a, b) (a >= b ? a : b)

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


#endif // NUMERICAL_INCLUDED

