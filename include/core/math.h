// Simple math library for operations I need while not having access to the standard library.
// ... this should be fun :)
// This file is a central staging point for sub-includes for various math objects. Whatever it contains is directly
// is either too small to justify its own file or is very freshly under construction.

#ifndef CORE_MATH_INCLUDED
#define CORE_MATH_INCLUDED

#include "assert.h"
#include "std_types.h"
#include "memory.h"

// Sub-objects
#include "math/numerical.h"
#include "math/vec2.h"
#include "math/random.h"
#include "math/crypto.h"

#endif // CORE_MATH_INCLUDED