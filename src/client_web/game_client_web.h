// Main internal symbols declaration for the web client code.

#ifndef GAME_CLIENT_WEB_INCLUDED
#define GAME_CLIENT_WEB_INCLUDED

// Only functions marked WASM_EXPORT are exported (build uses -fvisibility=hidden).
#define WASM_EXPORT extern "C" __attribute__((visibility("default")))

#include "game_common/game_match.h"

#endif // GAME_CLIENT_WEB_INCLUDED
