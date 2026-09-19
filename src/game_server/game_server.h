#ifndef GAME_SERVER_INCLUDED
#define GAME_SERVER_INCLUDED

#include "core.h"
#include "game_common/game_match.h"

// Main symbols file for the game server implementation.
// Defines the actual game server structure and internals.

struct game_match;

// States a match slot can be in.
// Lifecycle goes Uninitialized -> Waiting -> In Lobby -> Match Ongoing -> Match Ended -> Awaiting Cleanup -> Waiting -> [...]
enum class MATCH_SLOT_STATE : ui8
{
	UNINITIALIZED,		// Slot was just created and requires initialization.
	WAITING,			// Slot is ready to accept a new match.
	IN_LOBBY,			// Match has not started yet and is accepting new player connections.
	MATCH_ONGOING,		// Match is actively being played / ticked.
	MATCH_ENDED,		// Match has ended and is exposing its analytics data for other systems before cleanup.
	AWAITING_CLEANUP,	// Match has ended, and the slot can be re-used after cleanup.
};

// Data associated to a match slot when it is currently waiting or in lobby, accepting players joining the game and
// changes to the match parameters before it is started.
struct match_slot_lobby
{
	// ... hold player identifiers, associated to a connected client.
	ui8 to_implement;
};

// Wraps memory and a match structure that can be in multiple states.
// Allows the use and re-use of a same span of memory for a match's entire lifecycle: building the lobby, starting the match, ticking it...
struct match_slot
{
	MATCH_SLOT_STATE state;

	mem_arena slot_memory; // Memory assigned to this slot.

	game_match_start_params match_params; // Parameters for the current or next match (valid when in lobby or in a match).

	union
	{
		match_slot_lobby* lobby;	// Valid when the slot state is non MATCH_*

		struct
		{
			game_match* match_ptr;
			ui64 last_tick_time;

		} match;					// Valid when the slot state is MATCH_*
	};
};

struct game_server_init_params
{
	ui8 match_slot_count;

	// Test mode parameters.
	bool run_test_scenario; // If set to true, the server will start, run a match scenario on its first tick, dump it to a specific file then shutdown.
	const char* test_scenario_dump_filename; // If set to run test scenario, this indicates what file to dump the match data into once done.
};

struct game_server
{
	game_server_platform* platform; // Host platform functionality this server is running on.

	game_server_init_params init_params;

	bool shutdown_triggered; // Should the server shutdown as soon as possible ?
	ui64 tick_count; // How many ticks this server has gone through in total.

	time_ms time_ms; // Last recorded time from tick.

	mem_arena main_memory; // Main memory allocator for the server.
	match_slot* match_slots; // Match slots management structures.
};

#endif // GAME_SERVER_INCLUDED
