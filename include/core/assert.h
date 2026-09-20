// Platform-independent simple assertion system.
// The platform program must implement the ASSERT_EXIT() and ASSERT_MSG functions.

#ifndef CORE_ASSERT_INCLUDED
#define CORE_ASSERT_INCLUDED

#include "std_types.h"

// Exit function used by ASSERT macro.
void ASSERT_EXIT_FUNC();
// Message & Exit function used by ASSERT_MSG macro.
void ASSERT_MSG_FUNC(const char* assertMsg, const char* filename, ui32 line, ...);

#define ASSERT(exp) if (!(exp)) ASSERT_EXIT_FUNC();

#define ASSERT_MSG(exp, fail_msg, ...) if (!(exp)) {			\
	ASSERT_MSG_FUNC((fail_msg), __FILE__, __LINE__, __VA_ARGS__);	\
}

#endif // CORE_ASSERT_INCLUDED