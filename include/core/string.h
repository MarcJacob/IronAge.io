// Simple string helpers library for use across the codebase.
// TODO(Marc): We do want to have proper string objects so this is mostly temporary code.

#ifndef CORE_STRING_INCLUDED
#define CORE_STRING_INCLUDED

#include "assert.h"
#include "cstring.h"

#include "memory.h"
#include "math.h"

// Need to get va_list intrinsics depending on compiler.
// TODO(Marc): It'd probably be useful to get more explicit macros about which compiler is in use or even which specific project we're compiling at some point.
#if __clang__ || __GNUC__
#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#elif _MSC_VER
#include "stdarg.h"
#endif

// N-length view of a non-owned character string.
struct ia_string_view
{
	const char* view_str;
	ui32 length;

	// Simple inlined implicit constructor so views can handily be created from program const strings.
	inline ia_string_view(const char* c_str) : view_str(c_str), length(ia_str_len(c_str)) {}
	inline ia_string_view(const char* str, ui32 len) : view_str(str), length(len) {}
	inline ia_string_view() : view_str(nullptr), length(0) {}

	inline bool is_empty() const { return length == 0; }

	inline const char& operator[](ui32 index) const { ASSERT(index < length); return view_str[index]; }
};

// N-length string structure that manages a length of char-interpreted memory.
// Since they are not null-terminated by nature, they must be manually null-terminated (or copied into a buffer >1 trailing zero) before use in C string functions.
struct ia_string
{
	// _str and length kept at the same place as view_str and length in string view 
	// so the ia_string can be directly interpreted as a string view (without having to make the pointer const).
	char* _str;
	ui32 length;

	ui32 _capacity;

	// Implicit ability to pass / create a string view over this.
	inline operator ia_string_view()
	{
		return ia_string_view(_str, length);
	}

	inline bool is_empty() const { return length == 0; }
	inline const char& operator[](ui32 index) const { ASSERT(index < length); return _str[index]; }
};

// Variant of the N-length ia_string in static format, useful for structures and such.
template<ui32 Capacity>
struct ia_static_string
{
	char _str[Capacity];
	ui32 length;

	inline ia_static_string()
	{
		ia_memzero(_str, Capacity);
		length = 0;
	}

	// Since we know where their memory is, static strings can

	inline ia_static_string(const ia_string_view& str)
	{
		*this = str;
	}

	inline ia_static_string& operator=(const ia_string_view& str)
	{
		ASSERT(str.length <= Capacity);

		length = str.length;
		ia_memcpy(_str, str.view_str, length);
		if (Capacity > length)
		{
			ia_memset(_str + length, 0, Capacity - length);
		}

		return *this;
	}

	inline ia_static_string(const char* c_str) : ia_static_string(ia_string_view(c_str)) {}

	inline operator ia_string()
	{
		return ia_string{
			._str = _str,
			.length = length,
			._capacity = Capacity
		};
	}

	inline operator ia_string_view() const
	{
		return ia_string_view(_str, length);
	}

	inline const char& operator[](ui32 index) const { ASSERT(index < length); return _str[index]; }
};

// Initializes a new string into a memory arena from an existing string view.
// If min_capacity is specified, will allocate enough memory regardless of how long the source string is.
// Returns whether string was successfully allocated and initialized.
static bool ia_string_new(mem_arena& memory, const ia_string_view& src_string, ia_string& out_string, ui32 min_capacity = 0)
{
	out_string = {};

	// Ensure capacity is at least equal to min_capacity (with 4 being the absolute minimum no matter what).
	ui32 capacity = ia_max(min_capacity, src_string.length);
	capacity = ia_max(4, capacity);

	char* char_mem = memory.alloc<char>(capacity);
	if (char_mem == nullptr)
	{
		return false;
	}

	out_string._str = char_mem;
	out_string.length = src_string.length;
	out_string._capacity = capacity;

	if (!src_string.is_empty())
	{
		ia_memcpy(out_string._str, src_string.view_str, src_string.length);
	}

	return true;
}

// Replaces the contents of an existing string with the contents of the source. The source string must be
// equal in size or smaller than target capacity (no reallocation).
// Safe to call if source is viewing the modified string itself, so long as scratch memory is passed.
static void ia_string_replace(ia_string& string, const ia_string_view& src_string, mem_arena* scratch_mem_ptr = nullptr)
{
	bool sourceViewsTarget = (src_string.view_str + src_string.length) > string._str 
								&& src_string.view_str < (string._str + src_string.length); // Use source len in both comparisons so we can replace early part of a string with its late part.

	ASSERT(string._capacity >= src_string.length);
	ASSERT_MSG(scratch_mem_ptr != nullptr || !sourceViewsTarget, "Source views target memory, but no scratch memory was provided.");

	if (!sourceViewsTarget)
	{
		// Simple case, just copy and change length.
		string.length = src_string.length;
		ia_memcpy(string._str, src_string.view_str, src_string.length);
		return;
	}

	ui64 scratchMemStart = scratch_mem_ptr->allocated_count;

	// Copy source into scratch memory.
	ia_string source;
	if (!ia_string_new(*scratch_mem_ptr, src_string, source))
	{
		ASSERT_MSG(0, "Out of scratch memory in string_replace.");
		return;
	}

	// Perform copy into target.
	string.length = source.length;
	ia_memcpy(string._str, source._str, source.length);

	// Free scratch memory copy.
	scratch_mem_ptr->allocated_count = scratchMemStart;
}

// Removes the specified number of characters from the string, starting from the first.
// Returns number of chopped characters.
static ui32 ia_string_chop_left(ia_string& string, ui32 chop_count)
{
	ui32 chopCount = ia_min(chop_count, string.length);
	ia_memcpy(string._str, string._str + chopCount, string.length - chopCount);
	string.length -= chopCount;

    return chopCount;
}

// "Workaround" for chop right logic to also work on static strings.
template<ui32 Capacity>
static ui32 ia_string_chop_left(ia_static_string<Capacity>& string, ui32 chop_count)
{
	ia_string choppable = string;
	ui32 res = ia_string_chop_left(choppable, chop_count);

	string.length = choppable.length;
    return res;
}

// Removes the specified number of characters from the string view, starting from the last.
static ui32 ia_string_chop_right(ia_string& string, ui32 chop_count)
{
	ui32 chopCount = ia_min(chop_count, string.length);
	string.length -= chopCount;

    return chopCount;
}

// "Workaround" for chop right logic to also work on static strings.
template<ui32 Capacity>
static ui32 ia_string_chop_right(ia_static_string<Capacity>& string, ui32 chop_count)
{
	ia_string choppable = string;
	ui32 res = ia_string_chop_right(choppable, chop_count);

	string.length = choppable.length;

    return res;
}

// Removes the specified number of characters from the string view, starting from the first.
static ui32 ia_string_chop_left(ia_string_view& view, ui32 chop_count)
{
	ui32 chopCount = ia_min(chop_count, view.length);
	view.view_str += chopCount;
	view.length -= chopCount;

    return chopCount;
}

// Chops left on the string view until the specified character is encountered.
// If the character is not present in the string, no characters are chopped.
static ui32 ia_string_chop_left_until(ia_string_view& view, char until_char, bool chop_char = true)
{
    ui32 index = 0;
    while(index < view.length && view[index] != until_char) index++;

    return ia_string_chop_left(view, index + chop_char);
}

// Removes the specified number of characters from the string view, starting from the last.
static ui32 ia_string_chop_right(ia_string_view& view, ui32 chop_count)
{
	ui32 chopCount = ia_min(chop_count, view.length);
	view.length -= chopCount;

    return chopCount;
}

// Chops right on the string view until the specified character is encountered.
// If the character is not present in the string, no characters are chopped.
static ui32 ia_string_chop_right_until(ia_string_view& view, char until_char, bool chop_char = true)
{
    ui32 index = view.length - 1;
    while(index > 0 && view[index] != until_char) index--;

    return ia_string_chop_right(view, view.length - 1 - index + chop_char);
}

// Adds new characters to an existing string. The string must have the required capacity.
// Returns number of characters pushed. If must_full_push is true, either none or all the characters get pushed.
static ui32 ia_string_append(ia_string& string, const ia_string_view& chars, bool must_full_push = true)
{
	if (chars.is_empty()) return 0;

	ui32 newLen = string.length + chars.length;
	ui32 pushLen = chars.length;

	if (string._capacity < newLen)
	{
		if (must_full_push)
			return 0;

		pushLen = string._capacity - string.length;
		newLen = string._capacity;
	}

	ia_memcpy(string._str + string.length, chars.view_str, pushLen);
	string.length = newLen;

	return pushLen;
}

// "Workaround" for append logic to also work on static* strings.
template<ui32 Capacity>
static ui32 ia_string_append(ia_static_string<Capacity>& string, const ia_string_view& chars, bool must_full_push = true)
{
	ia_string appendable = string;
	ui32 res = ia_string_append(appendable, chars, must_full_push);

	string.length = appendable.length;
	return res;
}


static ia_string ia_string_format_v(const ia_string_view& format, mem_arena* write_mem, va_list va_args);
static ia_string ia_string_format(const ia_string_view& format, mem_arena* write_mem, ...);

// Wrapper for a string being built and the target memory arena.
// Passed for brevity in most string editing functions that involve changing the string's capacity.
// Assumes that it has exclusive ownership of the arena until done !
struct ia_string_builder
{
	ia_string string;

	mem_arena* _mem;

	ia_string_builder(mem_arena* arena, ui32 start_capacity = 0) : _mem(arena)
	{
		ASSERT(_mem != nullptr);
		ia_string_new(*_mem, "", string, start_capacity);
		ASSERT(string._str != nullptr);
	}

	// Has the builder pre-allocate capacity. Can be useful to do before performing a bunch of small pushes.
	inline bool push_capacity(ui32 cap)
	{
		// The extra bytes are assumed to be allocated right after current memory, so we just have to increment string capacity.
		char* loc = _mem->alloc<char>(cap); 
		if (loc == nullptr) return false;

		ASSERT(loc == string._str + string._capacity);

		string._capacity += cap;
		return true;
	}

	// Appends characters to the end of the string. The string is resized to fit as necessary.
	// TODO: Add a Formatted version.
	inline void push_back(const ia_string_view& str)
	{
		ASSERT(_mem != nullptr);
		if (string._capacity < string.length + str.length)
		{
			ui32 allocSize = (string.length + str.length - string._capacity);
			if (!push_capacity(allocSize)) return;
		}
		ia_string_append(string, str);
	}

	// Appends a single character to the back of the string. The string is resized to fit as necessary.
	inline void push_back(char character)
	{
		ASSERT(_mem != nullptr);
		if (string._capacity == string.length)
		{	
			if (!push_capacity(1)) return;
		}

		string._str[string.length++] = character;
	}

	inline void push_back_format(const ia_string_view& format, ...)
	{
		// Make sure capacity is no greater than length.
		if (string._capacity > string.length)
		{
			// Shrink to fit.
			ui32 shrinkage = string._capacity - string.length;
			_mem->allocated_count -= shrinkage;
			string._capacity = string.length;
		}

		va_list va_args;
		va_start(va_args, format);
		ia_string pushed = ia_string_format_v(format, _mem, va_args);
		va_end(va_args);

		// Can simply increase length to encompass pushed string.
		string._capacity += pushed._capacity;
		string.length += pushed.length;
	}

	// Appends characters to the front of the string. The string is resized to fit as necessary.
	inline void push_front(const ia_string_view& str)
	{
		ASSERT(_mem != nullptr);
		if (string._capacity < string.length + str.length)
		{
			ui32 allocSize = (string.length + str.length - string._capacity);
			if (!push_capacity(allocSize)) return;
		}

		ui32 finalLength = string.length + str.length;

		// Copy manually from the back of the string to avoid overwriting bytes as earlier ones are copied.
		for (ui32 i = 0; i < string.length; i++)
		{
			string._str[finalLength - 1 - i] = string._str[string.length - 1 - i];
		}

		// Copy pushed string into beginning of target memory.
		ia_memcpy(string._str, str.view_str, str.length);
	}

	// Appends a single character to the front of the string. The string is resized to fit as necessary.
	inline void push_front(char character)
	{
		ASSERT(_mem != nullptr);
		if (string._capacity < string.length + 1)
		{
			ui32 allocSize = (string.length + 1 - string._capacity);
			if (!push_capacity(allocSize)) return;
		}

		ui32 finalLength = string.length + 1;

		// Copy manually from the back of the string to avoid overwriting bytes as earlier ones are copied.
		for (ui32 i = 0; i < string.length; i++)
		{
			string._str[finalLength - 1 - i] = string._str[string.length - 1 - i];
		}

		string._str[0] = character;
	}

	// Removes characters from the beginning of the string.
	// If shrink_capacity is true, string capacity is set to exactly enough for the new length.
	inline void chop_left(ui32 char_count, bool shrink_capacity = false)
	{
		ASSERT(_mem != nullptr);

		ia_string_chop_left(string, char_count);

		if (shrink_capacity)
		{
			ui32 shrinkSize = string._capacity - string.length;	
			_mem->allocated_count -= shrinkSize;
			string._capacity = string.length;
		}
	}

	// Removes characters from the end of the string.
	// If shrink_capacity is true, string capacity is set to exactly enough for the new length.
	inline void chop_right(ui32 char_count, bool shrink_capacity = false)
	{
		ASSERT(_mem != nullptr);

		ia_string_chop_right(string, char_count);

		if (shrink_capacity)
		{
			ui32 shrinkSize = string._capacity - string.length;
			_mem->allocated_count -= shrinkSize;
			string._capacity = string.length;
		}
	}
};

// Builds a string representation of the passed value in the specified base (from 2 to 16) into the memory.
// The string is returned in ia_string format.
template<typename Numeric>
static ia_string ia_string_from_integer(mem_arena& memory, Numeric val, ui8 base = 10)
{
	ia_string_builder builder(&memory, 8); // Start builder with reasonable memory given the value and base.

	// Work on the unsigned magnitude. The sign is checked without comparing to 0 so unsigned types don't trip a tautological compare.
	bool isNegative = val != 0 && !(val > 0);
	ui64 magnitude = isNegative ? 0 - (ui64)val : (ui64)val;

	// Build the string in reverse order, then reverse it.
	do
	{
		builder.push_back(ia_digit_to_char((ui8)(magnitude % base)));
		magnitude /= base;
	} while (magnitude > 0);

	if (isNegative)
	{
		builder.push_back('-');
	}

	ia_str_reverse(builder.string._str, builder.string._str + builder.string.length - 1);

	return builder.string;
}

// Builds a string representation of the passed float value into the memory.
// precision is the number of digits of the fractional part to print.
// The string is returned in ia_string format.
// TODO(Marc): Currently does not print digits in the fractional part, so clearly not very useful.
static ia_string ia_string_from_float(mem_arena& memory, float val /*, ui8 precision*/)
{
	ia_string_builder builder(&memory, 8); // Start builder with reasonable memory given the value and base.

	// Build the string in reverse order, then reverse it.
	// TODO: Specify max total character count and print fractional part.
	do
	{
		ui8 digit = (i32)val % 10;
		builder.push_back(ia_digit_to_char(digit));
		val = val / 10.f;
	} while (val > 0.1f);

	ia_str_reverse(builder.string._str, builder.string._str + builder.string.length - 1);

	return builder.string;
}


static bool operator==(const ia_string_view& str_a, const char* str_b)
{
	ASSERT(str_b != nullptr);

	for (ui32 i = 0; i < str_a.length; i++)
	{
		// TODO(Marc): Optimize with multi-byte comparison if string comparisons ever end up being a performance pain point,
		// although I assume the compiler is probably already doing it for us.

		// Stop at str_b's terminator so we never read past it.
		if (str_b[i] == '\0' || str_a[i] != str_b[i]) return false;
	}

	// Every character of str_b up to str_a.length was non-null, so this index is within str_b.
	return str_b[str_a.length] == '\0';
}

static inline bool operator!=(const ia_string_view& str_a, const char* str_b)
{
	return !(str_a == str_b);
}

static inline bool operator==(const char* str_a, const ia_string_view& str_b)
{
	return str_b == str_a;
}

static inline bool operator!=(const char* str_a, const ia_string_view& str_b)
{
	return str_b != str_a;
}

static bool operator==(const ia_string_view& str_a, const ia_string_view& str_b)
{
	if (str_a.length != str_b.length) return false;

	for (ui32 i = 0; i < str_a.length; i++)
	{
		// TODO(Marc): Optimize with multi-byte comparison if string comparisons ever end up being a performance pain point,
		// although I assume the compiler is probably already doing it for us.

		if (str_a[i] != str_b[i]) return false;
	}

	return true;
}

// Check equality between two strings, with optional case-sensitivity.
static bool ia_string_equal(const ia_string_view& str, const ia_string_view& comp_str, bool case_sensitive = true)
{
	if (str.length != comp_str.length) return false;
	if (case_sensitive) return str == comp_str;

	for (ui32 i = 0; i < str.length; i++)
	{
		char str_char = str[i];
		char comp_char = comp_str[i];

		if (!case_sensitive)
		{
			constexpr char TO_UPPER_OFFSET = ('A' - 'a');
			if (str_char >= 'a' && str_char <= 'z') str_char += TO_UPPER_OFFSET;
			if (comp_char >= 'a' && comp_char <= 'z') comp_char += TO_UPPER_OFFSET;
		}

		if (str_char != comp_char) return false;
	}

	return true;

}

// Checks whether the string contains the specified string.
static bool ia_string_contains(const ia_string_view& str, const ia_string_view& contained, bool case_sensitive = true)
{
	if (contained.is_empty()) return true;

	ui32 contained_len = contained.length;
	if (contained_len > str.length) return false;
	
	ui32 scanIndex = 0;
	while (scanIndex <= str.length - contained_len)
	{
		ui32 match_len = 0;
		for (match_len = 0; match_len < contained_len; match_len++)
		{	
			char str_char = str[scanIndex + match_len];
			char comp_char = contained[match_len];

			if (!case_sensitive)
			{
				constexpr char TO_UPPER_OFFSET = ('A' - 'a');
				if (str_char >= 'a' && str_char <= 'z') str_char += TO_UPPER_OFFSET;
				if (comp_char >= 'a' && comp_char <= 'z') comp_char += TO_UPPER_OFFSET;
			}

			if (str_char != comp_char) 
			{
				break;
			}
		}

		if (match_len == contained_len) return true;
		scanIndex++;
	}

	return false;
}

// Returns whether string starts with chars.
static bool ia_string_starts_with(const ia_string_view& string, const ia_string_view& chars)
{
	if (string.length < chars.length) return false;
	if (chars.length == 0) return true;

	for (ui32 i = 0; i < chars.length; i++)
	{
		if (string[i] != chars[i]) return false;
	}

	return true;
}

// Returns whether string ends with chars.
static bool ia_string_ends_with(const ia_string_view& string, const ia_string_view& chars)
{
	if (string.length < chars.length) return false;
	if (chars.length == 0) return true;

	for (ui32 i = 0; i < chars.length; i++)
	{
		if (string[string.length - chars.length + i] != chars[i]) return false;
	}

	return true;
}

// Returns a string view over the next "word" in the specified string, up to specified maximum length (ignored if 0)
// By default, accepted characters only include alphanumerics. Other characters can be allowed by adding them to the null-terminated special chars string.
static ia_string_view ia_string_get_word(const ia_string_view& str, ui32 max_len = 0, const char* allowed_special_chars = nullptr)
{
	if (str.is_empty()) return {};

	ui32 specialCharCount = allowed_special_chars != nullptr ? ia_str_len(allowed_special_chars) : 0;

	ui32 readCount = 0;
	bool nextCharValid = true;
	while((max_len == 0 || readCount < max_len) && readCount < str.length)
	{
		char nextChar = str[readCount];

		nextCharValid = (nextChar >= 'a' && nextChar <= 'z')
			||	(nextChar >= 'A' && nextChar <= 'Z')
			|| (nextChar >= '0' && nextChar <= '9');

		for (ui32 i = 0; !nextCharValid && i < specialCharCount; i++)
		{
			nextCharValid = (nextChar == allowed_special_chars[i]);
		}

		if (!nextCharValid) break;
		readCount++;
	}

	return ia_string_view(str.view_str, readCount);
}

// Returns a string view over every next character until string terminator or end_char is reached.
// Does NOT include the end character.
static ia_string_view ia_string_get_until(const ia_string_view& str, char end_char, ui32 max_len = 0)
{
	if (str.is_empty()) return {};

	ui32 len = 0;
	while (len < str.length && str[len] != end_char && (max_len == 0 || len < max_len))
	{
		len++;
	}

	return ia_string_view(str.view_str, len);
}

// Creates a new string made up of the format string and the formatted parameters.
// Currently supports strings, floats and decimal integers.
// TODO(Portability)(Marc): This will probably crash horribly and start a trash fire on any other platform than Win32, so no-oping it for now.
static ia_string ia_string_format_v(const ia_string_view& format, mem_arena* write_mem, va_list va_args)
{
#ifndef _WIN32
	return {};
#endif

	if (format.length == 0) return {};

	ia_string_builder result(write_mem);

	ui32 vaCursor = 0;

	ui32 readCursor = 0;
	while (readCursor < format.length)
	{
		ia_string_view toNextSpecial = ia_string_get_until(ia_string_view(format.view_str + readCursor, format.length - readCursor), '%');
		readCursor += toNextSpecial.length;

		result.push_back(toNextSpecial);
		if (readCursor == format.length) break;

		ia_string_view fromSpecial = format;
		ia_string_chop_left(fromSpecial, readCursor);

		if (ia_string_starts_with(fromSpecial, "%llu"))
		{
			static_mem_arena<256> scratch;

			// Unsigned Integer decimal.
			ui64 val = *(ui64*)((ui8*)va_args + vaCursor);
			vaCursor += 8;

			ia_string str = ia_string_from_integer(scratch, val, 10);

			result.push_back(str);
			readCursor += 4;
			continue;
		}
		if (ia_string_starts_with(fromSpecial, "%d") || ia_string_starts_with(fromSpecial, "%i"))
		{
			static_mem_arena<256> scratch;

			// Signed Integer decimal.
			i32 val = *(i32*)((ui8*)va_args + vaCursor);
			vaCursor += 8;

			ia_string str = ia_string_from_integer(scratch, val, 10);

			result.push_back(str);
			readCursor += 2;
			continue;
		}
		if (ia_string_starts_with(fromSpecial, "%u"))
		{
			static_mem_arena<256> scratch;

			// Unsigned Integer decimal.
			ui32 val = *(ui32*)((ui8*)va_args + vaCursor);
			vaCursor += 8;

			ia_string str = ia_string_from_integer(scratch, val, 10);

			result.push_back(str);
			readCursor += 2 + ia_string_starts_with(fromSpecial, "%ud"); // Skip extra character if the long variation was used.
			continue;
		}
		if (ia_string_starts_with(fromSpecial, "%hu"))
		{
			static_mem_arena<256> scratch;

			// Unsigned Integer decimal.
			ui16 val = *(ui16*)((ui8*)va_args + vaCursor);
			vaCursor += 8;

			ia_string str = ia_string_from_integer(scratch, val, 10);

			result.push_back(str);
			readCursor += 3;
			continue;
		}
		/*if (ia_string_starts_with(fromSpecial, "%f")) // TODO(Marc)
		{
			static_mem_arena<256> scratch;

			// Unsigned Integer decimal.
			float val = *(float*)((ui8*)va_args + vaCursor);
			vaCursor += 8;
		}*/
		if (ia_string_starts_with(fromSpecial, "%s"))
		{
			// String view.
			ia_string_view* val = *(ia_string_view**)((ui8*)va_args + vaCursor);
			vaCursor += 8;

			result.push_back(*val);
			readCursor += 2;
			continue;
		}
		if (ia_string_starts_with(fromSpecial, "%cs"))
		{
			// Null-terminated string.
			const char* val = *(const char**)((ui8*)va_args + vaCursor);
			vaCursor += sizeof(val);

			result.push_back(val);
			readCursor += 3;
			continue;
		}

		ASSERT_MSG(0, "Unsupported format specifier. See fromSpecial value.");
	}

	return result.string;
}

static ia_string ia_string_format(const ia_string_view& format, mem_arena* write_mem, ...)
{
	va_list va_args;
	va_start(va_args, write_mem);
	ia_string res = ia_string_format_v(format, write_mem, va_args);
	va_end(va_args);

	return res;
}

#endif // CORE_STRING_INCLUDED
