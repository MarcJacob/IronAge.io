// Simple string helpers library for use across the codebase.
// TODO(Marc): We do want to have proper string objects so this is mostly temporary code.

#ifndef CORE_STRING_INCLUDED
#define CORE_STRING_INCLUDED

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

// Appends a null-terminated string to a fixed character buffer. Returns false if it didn't fit.
// append_count is incremented by how many characters were appended.
static bool ia_str_append(char* buff, ui32 buff_size, ui32& append_count, const char* str)
{
	while (*str != '\0')
	{
		if (append_count >= buff_size) return false;
		buff[append_count++] = *str++;
	}
	return true;
}

// Appends a "<name> (<tag>): " style prefix to a fixed character buffer, or just "<name>: " if tag is null or empty.
// Returns false if it didn't fit. append_count is incremented by how many characters were appended.
// NOTE(Marc): Yes, this is getting janky. Yes I need to stop being lazy and make a proper string library already.
static bool ia_str_prefix(char* buff, ui32 buff_size, ui32& append_count, const char* name, const char* tag)
{
	bool fit = ia_str_append(buff, buff_size, append_count, name);
	if (tag != nullptr && tag[0] != '\0')
	{
		fit = ia_str_append(buff, buff_size, append_count, " (") && fit;
		fit = ia_str_append(buff, buff_size, append_count, tag) && fit;
		fit = ia_str_append(buff, buff_size, append_count, ")") && fit;
	}
	return ia_str_append(buff, buff_size, append_count, ": ") && fit;
}

// Appends a ui64 integer to a fixed character buffer. Returns false if it didn't fit.
// Supports up to 20 digits. append_count is incremented with however many characters were added.
static bool ia_str_append_ui64(char* buff, ui32 buff_size, ui32& append_count, ui64 value)
{
	char digits[20];
	ui32 digitCount = 0;
	do
	{
		digits[digitCount++] = (char)('0' + (value % 10));
		value /= 10;
	} while (value > 0);

	while (digitCount > 0)
	{
		if (append_count >= buff_size) return false;
		buff[append_count++] = digits[--digitCount];
	}
	return true;
}

// Checks the given char string, returning true if its next characters corresond to the expected string.
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

#endif // CORE_STRING_INCLUDED
