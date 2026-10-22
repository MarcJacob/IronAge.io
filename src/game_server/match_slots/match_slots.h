// Symbol declarations for managing match slots on a game server.

#ifndef MATCH_SLOTS_INCLUDED
#define MATCH_SLOTS_INCLUDED

#include "../game_server_clients.h" // For game_server_client::client_handle.

#include "game_common/match/ai/match_ai.h"

// States a match slot can be in.
// Lifecycle goes Uninitialized -> Waiting -> In Lobby -> Match Ongoing -> Match Ended -> Awaiting Cleanup -> Waiting -> [...]
// The placement phase is not a state of its own: it is the in_placement flag of a Match Ongoing slot.
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
    ui64 alloc_start; // @TEMP(Marc): Where the lobby data allocation started, for clearing.
                      // Need to rethink the slot memory model.

    time_ms lobby_state_refresh_time;

	// Lobby countdown, started by the first client attaching. Kept outside the union below, which is zeroed at match start.
	bool countdown_active;
	time_ms countdown_end_ms; // Server uptime at which the match starts. Valid when countdown_active.
};

// Associates a player index (its position in a slot's players array) with the client currently controlling it, if any.
// A stale client handle (game_server_get_client_data returns nullptr for it) means the index has no live controller.
struct match_player
{
	bool bIsClientPlayer; // Whether this slot is occupied by a Game Client player or not.
	union
	{
		game_server_client::client_handle client;
		ai_player_state* AI;
	};
};

// Wraps memory and a match structure that can be in multiple states.
// Allows the use and re-use of a same span of memory for a match's entire lifecycle: building the lobby, starting the match, ticking it...
struct match_slot
{
	MATCH_SLOT_STATE state;

	mem_arena slot_memory; // Memory assigned to this slot.

	game_match_start_params* match_params; // Parameters for the current or next match (valid when in lobby or in a match).

	match_player* players; // Array of match_params->player_count entries, indexed by player index. Allocated once match_params is known.
    ui16 connection_count; // Counts the number of clients connected to the match.

	union
	{
		match_slot_lobby* lobby;	// Valid when the slot state is WAITING or IN_LOBBY.

		struct
		{
			game_match* match_ptr;

            bool in_placement;
            time_ms start_time;

            world_location* start_locations;

			time_ms last_tick_time;
            mem_arena next_tick_commands_memory;
            match_tick_commands_builder next_tick_commands_builder;

			ui16 ai_player_count; 
			ai_player_state* ai_players;

		} match;					// Valid when the slot state is MATCH_*
	};
};

using match_slot_index = ui8;
static constexpr match_slot_index INVALID_MATCH_SLOT_INDEX = ~0;

// Game Server functionality

struct game_server;

static constexpr time_ms LOBBY_COUNTDOWN_MS = 30000; // Time between the first client joining a lobby and its match starting. @TODO(Marc): Server config.
static constexpr time_ms PLACEMENT_COUNTDOWN_MS = 15000; // Time between start of the match and first tick during which players can pick a start location. @TODO(Marc): Server config.

static constexpr ui32 MATCH_END_CHECK_INTERVAL_TICKS = 30; // Placeholder end condition is checked every this many ticks.
static constexpr ui64 MATCH_END_POPULATION_PERCENT = 80; // Placeholder end condition: one player controls this much of the world population.

// Attaches a client as the controller of a free player index in the slot. Fails if the slot isn't in lobby. Writes the assigned index to out_player_index on success.
bool game_server_slot_attach_client(game_server& server, match_slot_index slot_index, game_server_client::client_handle client_handle, ui16& out_player_index);

// Detaches the client from the match slot if it is attached, freeing its player index. In a match the index stays reserved, with no input.
// Returns whether the client was actually attached to the slot.
bool game_server_slot_detach_client(game_server& server, match_slot_index slot_index, game_server_client::client_handle client_handle);

// Handles a game message from the client. Returns whether the message was handled.
bool game_server_match_slot_handle_message(game_server& server, game_server_client::client_handle client_handle, const game_message_header& msg);

// Sends a message to all game clients currently attached to the slot.
void game_server_slot_broadcast(game_server& server, match_slot& slot, const game_message_header& msg);

// LOBBY STATE FUNCTIONS

// Tick logic for a slot in LOBBY state.
void game_server_tick_match_slot_lobby(game_server& server, ui8 slot_index);

// ONGOING STATE FUNCTIONS

// Handler for CLIENT_PLACEMENT_PICK message.
void match_slot_handle_message_client_placement_pick(game_server& server, match_slot_index slot_index, game_server_client* player_client, const game_message_header& msg);

// Handler for CLIENT_TICK game message.
void match_slot_handle_message_client_tick(game_server& server, match_slot_index slot_index, game_server_client* player_client, const game_message_header& msg);

// Tick logic for a slot in ONGOING state.
void game_server_tick_match_slot_ongoing(game_server& server, ui8 slot_index);

// SLOT LIFECYCLE FUNCTIONS

// Initializes a new match slot in the server from a piece of memory to use and the slot index to initialize.
// The slot must currently be uninitialized.
// If successful, the passed memory arena is now owned by the slot itself. The one passed as param should be discarded.
// MUST ONLY BE CALLED ONCE PER SLOT ! From there RESET the slot for re-use.
bool game_server_init_match_slot(game_server& server, mem_arena& slot_mem, match_slot_index slot_index);

// Opens a slot's lobby in order to start accepting prospective players.
bool game_server_open_lobby(game_server& server, match_slot_index slot_index);

// Begins the match associated to the slot with the slot's current parameters.
bool game_server_start_match_slot(game_server& server, match_slot_index slot_index);

// Sets a match slot as having ended and notifies attached clients with MATCH_ENDED. During that time the match is no longer ticking
// but its state is still available for score-keeping and reporting.
// winner_id is INVALID_MATCH_PLAYER_ID if there is none.
bool game_server_end_match_slot(game_server& server, match_slot_index slot_index, match_player_id winner_id);

// Resets a match slot (ended, or awaiting cleanup after a failed start) to its cleaned state for re-use. Detaches any remaining clients.
bool game_server_reset_match_slot(game_server& server, match_slot_index slot_index);

#endif // MATCH_SLOTS_INCLUDED
