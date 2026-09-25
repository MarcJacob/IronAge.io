// Simple string helpers library for use across the codebase.
// TODO(Marc): We do want to have proper string objects so this is mostly temporary code.

#ifndef CORE_STRING_INCLUDED
#define CORE_STRING_INCLUDED

#include "assert.h"

// BEGIN NULL-TERMINATED STRING FUNCTIONS

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

// END NULL-TERMINATED STRING FUNCTIONS

// BEGIN SIZED-STRING FUNCTIONS

#include "memory.h"
#include "math.h"

// N-length view of a non-owned character string.
struct ia_string_view
{
	const char* view_str;
	ui32 length;

	// Simple inlined implicit constructor so views can handily be created from program const strings.
	inline ia_string_view(const char* c_str) : view_str(c_str), length(ia_str_len(c_str)) {}
	inline ia_string_view(const char* str, ui32 len) : view_str(str), length(len) {}
	inline ia_string_view() : view_str(nullptr), length(0) {}
};

// N-length string structure that manages a length of char-interpreted memory.
// Since they are not null-terminated by nature, they must be manually null-terminated (or copied into a buffer >1 trailing zero) before use in C string functions.
struct ia_string
{
	ui32 length;

	ui32 _capacity;
	char* _str;

	operator ia_string_view()
	{
		return ia_string_view(_str, length);
	}
};

// Variant of the N-length string in static format, useful for structures and such.
// Can be implicitly interpreted as a ia_string.
template<ui32 Capacity>
struct ia_static_string
{
	char _str[Capacity];
	ui32 length;

	operator ia_string()
	{
		return ia_string{
			.length = length,
			._capacity = Capacity,
			._str = _str
		};
	}

	operator ia_string_view()
	{
		return ia_string_view(_str, length);
	}
};

// Initializes a new string into a memory arena from an existing C string.
// If min_capacity is specified, will allocate enough memory regardless of how long the source string is.
// Returns whether string was successfully allocated and initialized.
static bool ia_string_new(mem_arena& memory, const char* src_str, ia_string& out_string, ui32 min_capacity = 0)
{
	ASSERT(src_str != nullptr);

	out_string = {};

	ui32 srcLen = ia_str_len(src_str);

	// Ensure capacity is at least equal to min_capacity (with 4 being the absolute minimum no matter what).
	ui32 capacity = ia_max(min_capacity, srcLen);
	capacity = ia_max(4, capacity);

	char* char_mem = memory.alloc<char>(capacity);
	if (char_mem == nullptr)
	{
		return false;
	}

	out_string._str = char_mem;
	out_string.length = srcLen;
	out_string._capacity = capacity;

	return true;
}

// Adds new characters to an existing string. The string must have the required capacity.
// Returns number of characters pushed. If must_full_push is true, either none or all the characters get pushed.
// NOTE(Marc): For now I am deciding on a string policy where you only get one chance to specify their capacity.
// A new type of string can be created later with the ability to dynamically request more memory / be reallocated through a function pointer or something.
// This also has the nice effect of making this function work seamlessly with static strings.
static ui32 ia_string_push(ia_string& string, const char* new_chars, bool must_full_push = false)
{
	ASSERT(new_chars != nullptr);

	ui32 pushLen = ia_str_len(new_chars);
	if (pushLen == 0) return 0;

	ui32 newLen = string.length + pushLen;

	if (string._capacity < newLen)
	{
		if (must_full_push)
			return 0;

		pushLen = string._capacity - string.length;
		newLen = string._capacity;
	}

	ia_memcpy(string._str + string.length, new_chars, pushLen);
	string.length = newLen;

	return pushLen;
}

template<ui32 StaticStringCapacity>
static ui32 ia_string_push(ia_static_string<StaticStringCapacity>& string, const char* new_chars, bool must_full_push = false)
{
	ia_string pushable = string;
	ui32 pushed = ia_string_push(pushable, new_chars, must_full_push);
	string.length += pushed;
	return pushed;
}

// Comparator with null-terminated C string.
static bool operator==(const ia_string_view& str_a, const char* str_b)
{
	ASSERT(str_b != nullptr);

	for (int i = 0; i < str_a.length; i++)
	{
		// TODO(Marc): Optimize with multi-byte comparison if string comparisons ever end up being a performance pain point,
		// although I assume the compiler is probably already doing it for us.

		if ((str_a.view_str[i] != str_b[i]) || (str_b[i] == '\0' && i != (str_a.length - 1))) return false;
	}

	return str_b[str_a.length] == '\0';
}

static inline bool operator==(const char* str_a, const ia_string_view& str_b)
{
	return str_b == str_a;
}

static bool operator==(const ia_string_view& str_a, const ia_string_view& str_b)
{
	if (str_a.length != str_b.length) return false;

	for (int i = 0; i < str_a.length; i++)
	{
		// TODO(Marc): Optimize with multi-byte comparison if string comparisons ever end up being a performance pain point,
		// although I assume the compiler is probably already doing it for us.

		if (str_a.view_str[i] != str_b.view_str[i]) return false;
	}

	return true;
}

// Check equality between a string and a null-terminated string, with optinal case-sensitivity.
static bool ia_string_equal(const ia_string_view& str, const ia_string_view& comp_str, bool case_sensitive = true)
{
	if (str.length != comp_str.length) return false;
	if (case_sensitive) return str == comp_str;

	for (int i = 0; i < str.length; i++)
	{
		char str_char = str.view_str[i];
		char comp_char = comp_str.view_str[i];

		if (!case_sensitive)
		{
			constexpr char TO_UPPER_OFFSET = ('A' - 'a');
			if (str_char >= 'a' && str_char <= 'z') str_char += TO_UPPER_OFFSET;
			if (comp_char >= 'a' && comp_char <= 'z') comp_char += TO_UPPER_OFFSET;
		}

		if ((str_char != comp_char) || (comp_char == '\0' && i != (str.length - 1))) return false;
	}

	return true;

}

// Returns a string view over the next "word" in the specified string, up to specified maximum length (ignored if 0)
// By default, accepted characters only include alphanumerics. Other characters can be allowed by adding them to the null-terminated special chars string.
static ia_string_view ia_string_get_word_n(const char* str, ui32 str_len, ui32 max_len = 0, const char* allowed_special_chars = nullptr)
{
	ASSERT(str != nullptr);
	if (str_len == 0) return ia_string_view(str, 0);

	ui8 specialCharCount = allowed_special_chars != nullptr ? ia_str_len(allowed_special_chars) : 0;

	ui16 readCount = 0;
	bool nextCharValid = true;
	while((max_len == 0 || readCount < max_len) && readCount < str_len)
	{
		char nextChar = str[readCount];

		nextCharValid = (nextChar >= 'a' && nextChar <= 'z')
			||	(nextChar >= 'A' && nextChar <= 'Z')
			|| (nextChar >= '0' && nextChar <= '9');

		for (int i = 0; !nextCharValid && i < specialCharCount; i++)
		{
			nextCharValid = (nextChar == allowed_special_chars[i]);
		}

		if (!nextCharValid) break;
		readCount++;
	}

	return ia_string_view(str, readCount);

}

// Returns a string view over the next "word" in the specified null-terminated string, up to specified maximum length (ignored if 0)
// By default, accepted characters only include alphanumerics. Other characters can be allowed by adding them to the null-terminated special chars string.
static ia_string_view ia_string_get_word(const char* str, ui32 max_len = 0, const char* allowed_special_chars = nullptr)
{
	return ia_string_get_word_n(str, ia_str_len(str), max_len, allowed_special_chars);
}

// Returns a string view over the next "word" in the specified string view, up to specified maximum length (ignored if 0)
// By default, accepted characters only include alphanumerics. Other characters can be allowed by adding them to the null-terminated special chars string.
static ia_string_view ia_string_get_word(const ia_string_view& str, ui32 max_len = 0, const char* allowed_special_chars = nullptr)
{
	return ia_string_get_word_n(str.view_str, str.length, max_len, allowed_special_chars);
}

// Returns a string view over every next character until string terminator or end_char is reached.
static ia_string_view ia_string_get_until(const char* str, char end_char, ui32 max_len = 0)
{
	ASSERT(str != nullptr);

	ui32 len = 0;
	while (str[len] != '\0' && str[len] != end_char && (max_len == 0 || len < max_len))
	{
		len++;
	}

	return ia_string_view(str, len);
}

// END SIZED-STRING FUNCTIONS

#endif // CORE_STRING_INCLUDED
