// Core include for the Game Server Main Lifecycle code.
// Contains essential game server symbols and lifecycle functions for use by the platform to start and maintain a server app.

#include "core.h"

#ifdef GAME_SERVER_MAIN_INCLUDED
	static_assert(0, "Game server main already included... do you have two platforms linking at the same time ?");
#else
#define GAME_SERVER_MAIN_INCLUDED
#endif

// Platform capabilities handed to the server code so it can, non-exhaustively:
// - Log messages
// - Read & Write files
// - Establish network connections
// - Create threads
// ...
// The platform for the game server specifically does not allow memory allocations. The memory the server has to work with is given on initialization.
struct game_server_platform
{
	// PLATFORM CONTROL

	typedef void (*shutdown_func)(int code);
	// Platform function: Requests standard shutdown when the game server has stopped.
	shutdown_func shutdown;

	// PLATFORM LOGGING

	typedef void (*log_stdout_func)(const wchar_t*);
	// Platform function: Takes in a null-terminated string and transfers it to standard output.
	log_stdout_func log_stdout;

	typedef void (*logf_stdout_func)(const wchar_t*, ...);
	// Platform function: Takes in a null-terminated format string and format parameters and transfers it to standard output.
	logf_stdout_func logf_stdout;

	// Platform function: Takes in a null-terminated string and transfers it to error output.
	typedef void (*log_stderr_func)(const wchar_t*);
	log_stderr_func log_stderr;
};


struct game_server;

/**
 * Initializes a new game server from the provided platform functions, giving it its memory footprint.
 * If successful, returns pointer to the initialized server structure. 
 * Update over time using game_server_tick.
 */
game_server* game_server_init(game_server_platform& platform, ui8* memory, ui64 memory_size);

/**
 * Integrates the passage of time into the game server simulation.
 */
void game_server_tick(game_server_platform& platform, game_server& server, float deltatime);


