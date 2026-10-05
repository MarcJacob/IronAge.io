// Common symbols shared across the WASM Web Client platform / frontend.

#ifndef WASM_CLIENT_INCLUDED
#define WASM_CLIENT_INCLUDED

// Only functions marked WASM_EXPORT are exported (build uses -fvisibility=hidden).
#define WASM_EXPORT extern "C" __attribute__((visibility("default")))

#include "core.h"

class game_client;

// No default member initializers here on purpose: they'd make this struct's default constructor non-trivial,
// and this -Wl,--no-entry build re-runs non-trivial global constructors on every exported call (no single entry
// point to run them exactly once) - silently resetting WEB_CLIENT.backend to null on every call after the first.
// web_client_start() explicitly zeroes this via `WEB_CLIENT = {};` instead.
struct web_client_state
{
	mem_arena backend_memory;
	game_client* backend;
};

extern web_client_state WEB_CLIENT;

#endif // WASM_CLIENT_INCLUDED
