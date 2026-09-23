// Symbol declarations for managing match slots on a game server.

#ifndef MATCH_SLOTS_INCLUDED
#define MATCH_SLOTS_INCLUDED

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

// Game Server functionality

struct game_server;

// Initializes a new match slot in the server from a piece of memory to use and the slot index to initialize.
// The slot must currently be uninitialized.
// If successful, the passed memory arena is now owned by the slot itself. The one passed as param should be discarded.
// MUST ONLY BE CALLED ONCE PER SLOT ! From there RESET the slot for re-use.
bool game_server_init_match_slot(game_server& server, mem_arena& slot_mem, ui8 slot_index);

// Opens a slot's lobby in order to start accepting prospective players.
bool game_server_open_lobby(game_server& server, ui8 slot_index);

// Begins the match associated to the slot with the slot's current parameters.
bool game_server_start_match_slot(game_server& server, ui8 slot_index);

// Sets a match slot as having ended. During that time the match is no longer ticking but its state is still available
// for score-keeping and reporting.
bool game_server_end_match_slot(game_server& server, ui8 slot_index);

// Resets a match slot to its cleaned state for re-use.
bool game_server_reset_match_slot(game_server& server, ui8 slot_index);

#endif // MATCH_SLOTS_INCLUDED
