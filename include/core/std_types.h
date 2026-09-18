// Standard type definitions used across the project.

#ifndef STD_TYPES_INCLUDED
#define STD_TYPES_INCLUDED

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

#endif // STD_TYPES_INCLUDED