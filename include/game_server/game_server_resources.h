// Declaration file for Game Server resource management.
// Resources are basically just files that are loadable by the server following certain specifications.

#ifndef GAME_SERVER_RESOURCES_INCLUDED
#define GAME_SERVER_RESOURCES_INCLUDED

#include "core.h"

static constexpr ui16 GAME_SERVER_RESOURCE_PATH_MAX_LEN = 256;
using game_server_resource_path = ia_static_string<GAME_SERVER_RESOURCE_PATH_MAX_LEN>; // Simple container for a reasonably-sized resource file path. Sized, NOT necessarily null-terminated.

#endif // GAME_SERVER_RESOURCES_INCLUDED
