#ifndef GAME_SERVER_INCLUDED
#define GAME_SERVER_INCLUDED

#include "core.h"

// Main symbols file for the game server implementation.
// Defines the actual game server structure and internals.

struct game_match;

struct game_server
{
	mem_arena main_memory; // Main memory allocator for the server.
};

#endif // GAME_SERVER_INCLUDED
