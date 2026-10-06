// Platform-independent simple assertion system.
// The platform program must implement the ASSERT_EXIT() and ASSERT_MSG functions.

#ifndef CORE_ASSERT_INCLUDED
#define CORE_ASSERT_INCLUDED

#include "std_types.h"

// Contains the functions necessary to handle assertions.
// Assign to the global _ASSERTS_HANDLER pointer to have it do so.
extern struct _ASSERTION_HANDLER
{
    // Exit function used by ASSERT macro.
    void (*ASSERT_EXIT_FUNC)();
    // Message & Exit function used by ASSERT_MSG macro.
    void (*ASSERT_MSG_FUNC)(const char* assertMsg, const char* filename, ui32 line, ...);

} *_ASSERTION_HANDLER_PTR; // Globally-declared pointer to handler for assertions per-module. Must be assigned for assertions to work.

#if __clang__

#define _assert_break() __builtin_trap()

#if __has_builtin(__builtin_verbose_trap)
#define _assert_break_verbose(msg) __builtin_verbose_trap("", msg)
#else
#define _assert_break_verbose(msg) __builtin_trap()
#endif

#else
#define _assert_break() *((void*)NULL) = 0
#define _assert_break_verbose(msg) _assert_break()
#endif

#define ASSERT(exp)  \
    if (!(exp)) \
    { \
        if (_ASSERTION_HANDLER_PTR == nullptr || _ASSERTION_HANDLER_PTR->ASSERT_EXIT_FUNC == nullptr) _assert_break(); \
        else _ASSERTION_HANDLER_PTR->ASSERT_EXIT_FUNC(); \
    }

#define ASSERT_MSG(exp, fail_msg, ...) \
    if (!(exp)) \
    {			\
        if (_ASSERTION_HANDLER_PTR == nullptr || _ASSERTION_HANDLER_PTR->ASSERT_MSG_FUNC == nullptr) _assert_break_verbose(fail_msg); \
        else _ASSERTION_HANDLER_PTR->ASSERT_MSG_FUNC((fail_msg), __FILE_NAME__, __LINE__, ##__VA_ARGS__);	\
    }

#endif // CORE_ASSERT_INCLUDED
