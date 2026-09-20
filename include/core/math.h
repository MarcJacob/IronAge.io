// Simple math library for operations I need while not having access to the standard library.
// ... this should be fun :)

#ifndef CORE_MATH_INCLUDED
#define CORE_MATH_INCLUDED

#include "assert.h"
#include "std_types.h"
#include "memory.h"

struct sha1_result
{
	ui32 result[5];
	ui32 _padding[1];
};

static inline void endian_reverse_ui16(ui16* val)
{
	ui8* bytes = (ui8*)val;
	*val = ((ui16)*bytes << 8) | (ui16)*(bytes + 1);
	return;
}

static inline void endian_reverse_ui32(ui32* val)
{
	ui8* bytes = (ui8*)val;
	*val = ((ui32)*bytes << 24) | ((ui32)*(bytes + 1) << 16) | ((ui32)*(bytes + 2) << 8) | (ui32)*(bytes + 3);
	return;
}

static inline void endian_reverse_ui64(ui64* val)
{
	ui8* bytes = (ui8*)val;
	*val = ((ui64)*bytes << 56) | ((ui64)*(bytes + 1) << 48) | ((ui64)*(bytes + 2) << 40) | ((ui64)*(bytes + 3) << 32)
		| ((ui64)*(bytes + 4) << 24) | ((ui64)*(bytes + 5) << 16) | ((ui64)*(bytes + 6) << 8) | (ui64)*(bytes + 7);
	return;
}

static sha1_result ia_sha1(const ui8* data, ui64 data_size);

// Define HASH_PLATFORM_IMPLEM in platform code to implement your own optimized platform-specific version of the hashing functions.
#ifndef HASH_PLATFORM_IMPLEM

static sha1_result ia_sha1(const ui8* data, ui64 data_size)
{
	ASSERT(data != nullptr);
	ASSERT(data_size < (~(ui64)0) / 8); // Make sure data size is lower than an 8th of the max 64 bits value. This should be plenty for any real data.

	struct hash_state
	{
		ui32 h0, h1, h2, h3, h4;
		ui32 _padding;
	} hash; // Main hash we'll be accumulating into.

	typedef ui8 hash_datablock[64];

	// Start state by convention.
	hash.h0 = 0x67452301;
	hash.h1 = 0xEFCDAB89;
	hash.h2 = 0x98BADCFE;
	hash.h3 = 0x10325476;
	hash.h4 = 0xC3D2E1F0;

	ui64 fullBlockCount = data_size / 64;
	ui8 lastBlockSize = data_size % 64;
	
	// Build tail block(s) data.
	ui8 tail[128] = {0};
	bool tailDoubleBlock = false;
	ui64* msgLengthPtr = (ui64*)(tail + 56);
	if (lastBlockSize > 0)
	{
		ia_memcpy(tail, data + (data_size - lastBlockSize), lastBlockSize);
	}

	tail[lastBlockSize] = 0x80;

	if (lastBlockSize + 1 > 56)
	{
		// Tail has to span two blocks. Push the msg length all the way to the end of second tail block.
		msgLengthPtr = (ui64*)(tail + 120);
		tailDoubleBlock = true;
	}

	// Add message length to end of data with reversed endianness.
	*msgLengthPtr = data_size * 8;
	endian_reverse_ui64(msgLengthPtr);

	// Define compression function. Takes in a datablock and compresses it into the existing hash.
	auto compress = [&hash](const hash_datablock& datablock)
		{
			ui32 words[80];
			ui8* data_bytes = (ui8*)&datablock;

			// Initial load into first 16 words = switch endianness to big endian.
			for (ui8 wordIndex = 0; wordIndex < 16; wordIndex++)
			{
				words[wordIndex] = *(ui32*)(data_bytes + wordIndex * 4);
				endian_reverse_ui32(&words[wordIndex]);
			}

			for (ui8 wordIndex = 16; wordIndex < 80; wordIndex++)
			{
				words[wordIndex] = words[wordIndex - 3] ^ words[wordIndex - 8] ^ words[wordIndex - 14] ^ words[wordIndex - 16];
				words[wordIndex] = words[wordIndex] << 1 | (words[wordIndex] >> 31); // "Rotate" by 1 to the left.
			}

			hash_state subHash = hash;
			for (ui8 step = 0; step < 80; step++)
			{
				ui32 temp = (subHash.h0 << 5 | subHash.h0 >> 27) + subHash.h4 + words[step];
				if (step < 20)
				{
					temp += ((subHash.h1 & subHash.h2) | (~subHash.h1 & subHash.h3))
						+ 0x5A827999;
				}
				else if (step < 40)
				{
					temp += (subHash.h1 ^ subHash.h2 ^ subHash.h3)
						+ 0x6ED9EBA1;
				}
				else if (step < 60)
				{
					temp += ((subHash.h1 & subHash.h2) | (subHash.h1 & subHash.h3) | (subHash.h2 & subHash.h3))
						+ 0x8F1BBCDC;
				}
				else
				{
					temp += (subHash.h1 ^ subHash.h2 ^ subHash.h3)
						+ 0xCA62C1D6;
				}

				subHash.h4 = subHash.h3;
				subHash.h3 = subHash.h2;
				subHash.h2 = (subHash.h1 << 30 | subHash.h1 >> 2);
				subHash.h1 = subHash.h0;
				subHash.h0 = temp;
			}

			// Add compressed block to main hash.
			hash.h0 += subHash.h0;
			hash.h1 += subHash.h1;
			hash.h2 += subHash.h2;
			hash.h3 += subHash.h3;
			hash.h4 += subHash.h4;
		};

	// Process full data blocks
	for (ui64 fullBlockIndex = 0; fullBlockIndex < fullBlockCount; fullBlockIndex++)
	{
		hash_datablock* datablock = (hash_datablock*)(data + fullBlockIndex * 64);
		compress(*datablock);
	}

	// Process tail blocks
	hash_datablock* tail_datablock = (hash_datablock*)(tail);
	compress(*tail_datablock);

	if (tailDoubleBlock)
	{
		tail_datablock = (hash_datablock*)(tail + 64);
		compress(*tail_datablock);
	}

	sha1_result res = {};
	ia_memcpy(&res, &hash, 20);

	for (int resWord = 0; resWord < 5; resWord++)
	{
		endian_reverse_ui32(&res.result[resWord]);
	}

	return res;
}

#endif

#endif // CORE_MATH_INCLUDED