// Core project-wide symbols useable anywhere.

#ifndef CORE_INCLUDED
#define CORE_INCLUDED

#include "core/std_types.h"
#include "core/assert.h"
#include "core/memory.h"
#include "core/string.h"
#include "core/math.h"

#define SET_DEFAULT_VAL(value, default) \
    (value = (value != 0 ? (value) : ((__typeof(value))default)))

#define SET_DEFAULT_VAL_STRUCT(value, default) \
    (value = (STRUCT_IS_INITIALIZED(value) ? (value) : ((__typeof(value))default)))

#endif // CORE_INCLUDED
