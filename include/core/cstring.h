// Header library for manipulating C-style strings / null-terminated strings when the standard library is not available.
// Also includes general character string related manipulation functions that do not depend on intrinsic knowledge of string length or need to allocate anything.

#ifndef CORE_C_STRING_INCLUDED
#define CORE_C_STRING_INCLUDED

#include "std_types.h"
#include "assert.h"

// Returns true if the two strings are strictly equal.
static bool ia_str_equal(const char* a, const char* b)
{
	ASSERT(a != nullptr && b != nullptr);

	// Advance into both strings as long as end character isn't reached and characters keep being equal.
	while (*a != '\0' && *b != '\0'
		&& *a == *b) 
	{ 
		a++; 
		b++;
	}

	// Return whether both strings ended at the same time.
	return *a == *b;
}

// Returns the length of the given string without its null-terminator.
static ui32 ia_str_len(const char* str)
{
	ASSERT(str != nullptr);

	ui32 len = 0;
	while (*str != '\0')
	{
		len++;
		str++;
	}

	return len;
}

// Checks the given char string, returning true if its next characters correspond to the expected string.
static bool ia_str_expect(const char* str, const char* expected)
{
	ASSERT(str != nullptr && expected != nullptr);

	while (*str != '\0' && *expected != '\0'
		&& *str == *expected)
	{
		str++;
		expected++;
	}

	return *expected == '\0';
}

// Converts the given digit value to its character.
// Supports from 0 to 15 (included).
static char ia_digit_to_char(ui8 digit)
{
	ASSERT(digit < 16);

	// 0 - 9
	if (digit <= 9)
	{
		return '0' + digit;
	}

	// A - F
	if (digit <= 15)
	{
		return 'A' + (digit - 10);
	}

	return '\0';
}

// Reverses the characters from start to end (both included).
// Works for up to 65535 characters.
static void ia_str_reverse(char* start, char* end)
{
	ASSERT(start != nullptr && end != nullptr);

	ui32 charCount = (iptr)end - (iptr)start + 1;
	ASSERT(charCount < 65536);

	for (ui16 i = 0; i < charCount / 2; i++)
	{
		char temp = start[i];
		start[i] = *(end - i);
		*(end - i) = temp;
	}
}

#endif // CORE_C_STRING_INCLUDED

