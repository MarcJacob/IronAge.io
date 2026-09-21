// Standard type definitions used across the project.

#ifndef CORE_STD_TYPES_INCLUDED
#define CORE_STD_TYPES_INCLUDED

// Integers

typedef unsigned long long ui64;
typedef unsigned int ui32;
typedef unsigned short ui16;
typedef unsigned char ui8;

typedef long long i64;
typedef int i32;
typedef short i16;
typedef char i8;

#if _WIN64
typedef ui64 iptr;
#else
typedef ui32 iptr;
#endif

typedef ui64 time_ms; // Used on some apps as a way to measure time precisely.

// Severity / category of a log message.
enum LOG_TYPE
{
	LOG_SUCCESS,
	LOG_NORMAL,
	LOG_WARNING,
	LOG_ERROR,
};

#endif // CORE_STD_TYPES_INCLUDED