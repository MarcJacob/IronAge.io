#include "core.h"

#ifndef GAME_SERVER_INCLUDED
#define GAME_SERVER_INCLUDED

// Main symbols file for the game server implementation.
// Defines the actual game server structure and internals.

struct game_server
{
	ui8* memStart;
	ui64 memSize;
};

#endif // GAME_SERVER_INCLUDED
