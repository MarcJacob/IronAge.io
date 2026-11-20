// Declaration file for the platform-independent global Debug Logging service.

#ifndef LOGGING_INCLUDED
#define LOGGING_INCLUDED

#include "string.h"

// Debug logging service.
// @NOTE(Marc): For a long time I was thinking that keeping logging out of static memory was reasonable,
// but now I'm finding out it's really not. I need to re-conceptualize my boundary between what can be static / global
// and what can't. I think I'm converging on this: stuff that represent interfaces of the *program* as a whole can be
// made global (with the commitment that even conceptually they'll be taking a global spot), while things that need / benefit to 
// be kept "pure" in their logic (which is still the vast majority of things) remain non-global.
// Debug logging can be seen as many places in code converging on a single program-wide logging service. As such,
// I think I will transform the existing logging code into using a global logging handler like with assertions, except the game server
// (for example) will keep its own logging code for formatting purposes.

// Contains the functions necessary to handle debug logging.
// Assign to the global _DEBUG_LOG_HANDLER pointer to have it do so.
// @TODO(Marc): Transfer a bunch of messaging features like message categories here.
extern struct _DEBUG_LOG_HANDLER
{
    // Standard debug message function.
    void (*DEBUG_MSG_FUNC)(const ia_string_view& msg);

    // Formatted debug message function.
    void (*DEBUG_MSGF_FUNC)(const ia_string_view& msg, ...);

} *_DEBUG_LOG_HANDLER_PTR;  // Globally-declared pointer to handler for debug logging per-module.
                            // Must be assigned for logs to work.

#define DEBUG_LOG(msg)                              \
{                                                   \
    if (_DEBUG_LOG_HANDLER_PTR != nullptr)              \
    {                                               \
        _DEBUG_LOG_HANDLER_PTR->DEBUG_MSG_FUNC(msg);    \
    }                                               \
}

#define DEBUG_LOGF(format, ...)                                         \
{                                                                       \
    if (_DEBUG_LOG_HANDLER_PTR != nullptr)                              \
    {                                                                   \
        _DEBUG_LOG_HANDLER_PTR->DEBUG_MSGF_FUNC(format, __VA_ARGS__);   \
    }                                                                   \
}


#endif // LOGGING_INCLUDED
