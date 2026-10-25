// Symbol declaration for Game Message system, network-oriented message structures.

#ifndef GAME_MESSAGES_INCLUDED
#define GAME_MESSAGES_INCLUDED

#include "match/commands.h" // For match_tick_commands, embedded by value in the tick message payloads below.
#include "match/start_params.h"

// Enumaration of supported game message types.
// Each type features a high-level description of its functionality. More detail can be found atop the corresponding payload structure.
enum class GAME_MESSAGE_TYPE : ui8
{
	LOBBY_JOINED,		// Server -> Client = Client is in a lobby. Sent on joining and again whenever the lobby's player count changes.
	MATCH_STARTED,		// Server -> Client = Contains instructions on the match the client is in, including which player is controlled.
	MATCH_ENDED,		// Server -> Client = Signal of match ending on the server, with result information.

    CLIENT_PLACEMENT_PICK, // Client -> Server = Request to start the game at the provided location.
    SERVER_PLACEMENT_PICK, // Server -> Client = Indicate the given player will start the game at the specified location.

	SERVER_TICK,		// Server -> Client = Input sequence for a specific tick provided by the server to the client. Allows client to progress local match state.
	SERVER_TICK_BUNDLE, // Server -> Client = Multiple ticks worth of input sequences.
	CLIENT_TICK,

	REQUEST_AUTHENTICATION,	// Client -> Server = First message of a client: asks to be authenticated, with an optional name.
	AUTHENTICATED,			// Server -> Client = Authentication confirmed, with the name the server kept.
	SERVER_LOBBY_LIST,		// Server -> Client = Lobbies open to joining. Sent regularly to authenticated clients outside of any lobby.
	REQUEST_JOIN_LOBBY,		// Client -> Server = Asks to join a lobby from the list.
	JOIN_LOBBY_RESPONSE,	// Server -> Client = Answer to REQUEST_JOIN_LOBBY, always sent. On success LOBBY_JOINED messages follow.
	SIGNAL_LEAVE_LOBBY,		// Client -> Server = Client leaves its lobby, freeing its seat.
	LOBBY_CLIENT_KICKED,	// Server -> Client = Client was removed from its lobby by the server.

	LOBBY_ADMIN_START_MATCH,		// Client -> Server = Test lobbies only: start the match now.
	LOBBY_ADMIN_SET_PLAYER_COUNT,	// Client -> Server = Test lobbies only: set the player cap (humans above it are kicked).
	LOBBY_ADMIN_SET_MAP_SIZE,		// Client -> Server = Test lobbies only: set the world size in regions.
	LOBBY_ADMIN_SET_SEED,			// Client -> Server = Test lobbies only: set the match seed.

	TYPE_COUNT,
	INVALID,
};

// Have all structures below (message header and payloads) packed.
#pragma pack(push, 1)

// Defines the first portion of a game message.
// Use to interpret the following payload memory to the correct structure.
struct game_message_header
{
	GAME_MESSAGE_TYPE message_type;

	ui16 payload_size;
	ui8 _payload[];

	// Get a typed reference to the beginning of the payload bytes.
	// Asserts that the payload is at least large enough for the type of payload desired, but does NOT guarantee anything beyond that,
	// specifically the validity of the values or the coherence of the size of the payload for dynamically-sized messages.
	template<typename PayloadType>
	inline PayloadType& get_payload_ref() const { ASSERT(payload_size >= sizeof(PayloadType)); return *(PayloadType*)_payload; }
};

// BEGIN GAME SERVER CORE MESSAGES

static constexpr ui8 PLAYER_NAME_MAX_LENGTH = 32; // Characters in a player name, zero terminator excluded.
static constexpr ui8 LOBBY_LIST_MAX_COUNT = 16; // Lobbies a SERVER_LOBBY_LIST message can hold.

// Hard limits of the lobby admin commands (test lobbies).
static constexpr ui16 LOBBY_MAX_PLAYER_COUNT = 32; // Players in a match, humans and AI. Also the size of a lobby's player array.
static constexpr ui16 LOBBY_MAX_MAP_SIZE_REGIONS = 8; // World width and height, in regions.

// State of a lobby, as shown to clients in it (LOBBY_JOINED) and in the lobby list (SERVER_LOBBY_LIST).
// Read by the web client at fixed offsets (backend.ts).
struct game_lobby_state
{
	ui32 countdown_remaining_ms; // Time left before the match starts, in milliseconds. 0 when no countdown runs (always 0 in a test lobby).
	ui16 human_count; // Number of clients currently in the lobby. AI players fill the rest of match_start_params.player_count.
	ui8 lobby_id; // Identifies the lobby on the server (index of the match slot hosting it).

	game_match_start_params match_start_params; // Params the match will start with. player_count is the player cap, humans and AI together.

	bool test_mode; // Test lobby: never starts on its own, and every client in it may use the LOBBY_ADMIN_* messages.
};
static_assert(sizeof(game_lobby_state) == 25, "game_lobby_state size changed. Adapt backend.ts readers, then this assertion.");

// Server -> Client message payload.
// Received by a client as signal that it is in a lobby, with the lobby's state. The match starts when the countdown reaches 0.
// The client can count down locally from countdown_remaining_ms (relative to message reception), a new message corrects it.
// Sent on joining, whenever the lobby changes and every second.
struct game_message_payload_lobby_joined
{
	game_lobby_state lobby;
};

// Player names on the wire: a core static string (PLAYER_NAME_MAX_LENGTH chars, not zero-terminated, then its ui32 length).
// A received length above PLAYER_NAME_MAX_LENGTH is invalid: check it before using the string.
using player_name = ia_static_string<PLAYER_NAME_MAX_LENGTH>;
static_assert(sizeof(player_name) == PLAYER_NAME_MAX_LENGTH + 4, "player_name layout changed. Adapt backend.ts, then this assertion.");

// Client -> Server message payload.
// First message a client must send: no lobby action is accepted before the server answers with AUTHENTICATED.
struct game_message_payload_request_authentication
{
	player_name name; // Empty = the server picks a name.
};

// Server -> Client message payload.
// Confirms the authentication.
struct game_message_payload_authenticated
{
	player_name name; // Name the server kept for the client.
	ui8 auth_key[16]; // Reserved for quick reconnection. All zero for now.
};

// Server -> Client message payload.
// Lobbies currently open (in lobby state, full ones included). Only the first lobby_count entries are valid.
struct game_message_payload_server_lobby_list
{
	ui8 lobby_count;
	game_lobby_state lobbies[LOBBY_LIST_MAX_COUNT];
};

// Client -> Server message payload.
struct game_message_payload_request_join_lobby
{
	ui8 lobby_id; // game_lobby_state::lobby_id of the lobby to join.
};

enum class JOIN_LOBBY_RESULT : ui8
{
	SUCCESS,
	NOT_AUTHENTICATED, // The client has not been authenticated yet.
	INVALID_LOBBY, // No open lobby has this id.
	LOBBY_FULL, // Every player index of the lobby is taken by a human.
	ALREADY_IN_LOBBY, // The client is already in a lobby or match.
};

// Server -> Client message payload.
struct game_message_payload_join_lobby_response
{
	ui8 lobby_id; // Lobby the request was for.
	JOIN_LOBBY_RESULT result;
};

// Client -> Server message payload. No content.
struct game_message_payload_signal_leave_lobby
{
	ui8 _unused; // Payloads can't be empty.
};

enum class LOBBY_KICK_REASON : ui8
{
	PLAYER_CAP_LOWERED, // The lobby's player cap went below the client's player index.
};

// Server -> Client message payload. The client is no longer in the lobby and gets the lobby list again.
struct game_message_payload_lobby_client_kicked
{
	LOBBY_KICK_REASON reason;
};

// Read by the web client at fixed offsets (backend.ts): adapt its readers, then these assertions.
static_assert(sizeof(game_message_payload_authenticated) == 52, "Adapt backend.ts.");
static_assert(sizeof(game_message_payload_server_lobby_list) == 1 + 25 * LOBBY_LIST_MAX_COUNT, "Adapt backend.ts.");
static_assert(sizeof(game_message_payload_join_lobby_response) == 2, "Adapt backend.ts.");
static_assert(sizeof(game_message_payload_lobby_client_kicked) == 1, "Adapt backend.ts.");

// LOBBY ADMIN MESSAGES (Client -> Server). Only accepted from a client in a test lobby, applied to that lobby.

struct game_message_payload_lobby_admin_start_match
{
	ui8 _unused; // Payloads can't be empty.
};

struct game_message_payload_lobby_admin_set_player_count
{
	ui16 player_count; // Clamped to [1, LOBBY_MAX_PLAYER_COUNT].
};

struct game_message_payload_lobby_admin_set_map_size
{
	vec2<ui16> world_size_regions; // Each axis clamped to [1, LOBBY_MAX_MAP_SIZE_REGIONS].
};

struct game_message_payload_lobby_admin_set_seed
{
	ui32 random_seed;
};

// Server -> Client message payload.
// Received by a client as instruction by the server of what the match starting / current state is, and what player ID they are in control of.
struct game_message_payload_match_started
{
	ui32 placement_countdown_ms; // Indicative time in milliseconds before the placement phase ends (first SERVER_TICK), relative to message reception.

	ui32 join_tick; // What tick the match is currently on as the client joins. Currently this means "You will have to catch up to this tick" if above 0.
					// TODO: Allow this message (or an alternative form of it) to feature a snapshot of the game state to speedup mid-game joining process ?

	match_player_id controlled_player_id; // What player ID the client is in charge of controlling.

	game_match_start_params match_start_params; // Match start parameters. Don't forget it features post-structure bytes for dynamic world state elements !

	// Followed by match_start_params.player_count player_name entries (index = player ID, humans and AI, names as they were at match start).
	// So payload_size = sizeof(this) + player_count * sizeof(player_name), player_count <= LOBBY_MAX_PLAYER_COUNT.
};

// Server -> Client message payload.
// Received by a client as signal that the match they're in has ended and that the server will supply no further tick messages.
// The connection stays up: the server then puts the client back in a lobby (LOBBY_JOINED).
struct game_message_payload_match_ended
{
	match_player_id winner_id; // ID of the victorious player, INVALID_MATCH_PLAYER_ID if none.
};

// END GAME SERVER CORE MESSAGES

// BEGIN MATCH COMMAND MESSAGES

struct game_message_payload_placement_pick
{
    match_player_id player_id;
    world_location picked_loc;
};

// Server -> Client message payload.
// Received by a client as the authoritative set of input command sequences applied over a specific match tick.
struct game_message_payload_server_tick
{
	ui32 apply_tick; // Which tick the input sequence happened for.

	match_tick_commands commands; // Must stay last: match_tick_commands ends in a flexible array.
};

// Server -> Client message payload.
// Received by a client as the authoritative set of input command sequences applied over a range of match ticks.
// Should also be used as a signal by the server that the client is currently in a "catch up" state.
struct game_message_payload_server_tick_bundle
{
	ui32 apply_tick_start; // The tick the first sequence applies on.
	ui32 apply_tick_count; // Number of ticks / sequences to apply on following ticks.

	ui8 _match_tick_commands_buffer[];

	// Returns a typed reference of a bundle item struct at the provided byte offset.
	inline match_tick_commands& get_tick_commands_at(ui16 byte_offset) const
	{
		return *(match_tick_commands*)&_match_tick_commands_buffer[byte_offset];
	}
};

// Client -> Server message payload.
// Received by a server as the sequence of inputs a client wished to apply onto the match along
// with the tick they based their perception of the match state on.
struct game_message_payload_client_tick
{
	ui32 emit_tick; // Which tick the client's game state was at when the message was emitted.

	match_command_sequence commands; // Sequence of commands associated to the emitting player they wish to apply over the next match tick.
};

// END MATCH COMMAND MESSAGES

static inline ui16 get_message_type_payload_size(GAME_MESSAGE_TYPE type)
{
	switch (type)
	{
	case GAME_MESSAGE_TYPE::LOBBY_JOINED:
		return sizeof(game_message_payload_lobby_joined);

	case GAME_MESSAGE_TYPE::MATCH_STARTED:
		return sizeof(game_message_payload_match_started);

	case GAME_MESSAGE_TYPE::MATCH_ENDED:
		return sizeof(game_message_payload_match_ended);

    case GAME_MESSAGE_TYPE::CLIENT_PLACEMENT_PICK:
    case GAME_MESSAGE_TYPE::SERVER_PLACEMENT_PICK:
        return sizeof(game_message_payload_placement_pick);

	case GAME_MESSAGE_TYPE::SERVER_TICK:
		return sizeof(game_message_payload_server_tick);

	case GAME_MESSAGE_TYPE::SERVER_TICK_BUNDLE:
		return sizeof(game_message_payload_server_tick_bundle);

	case GAME_MESSAGE_TYPE::CLIENT_TICK:
		return sizeof(game_message_payload_client_tick);

	case GAME_MESSAGE_TYPE::REQUEST_AUTHENTICATION:
		return sizeof(game_message_payload_request_authentication);

	case GAME_MESSAGE_TYPE::AUTHENTICATED:
		return sizeof(game_message_payload_authenticated);

	case GAME_MESSAGE_TYPE::SERVER_LOBBY_LIST:
		return sizeof(game_message_payload_server_lobby_list);

	case GAME_MESSAGE_TYPE::REQUEST_JOIN_LOBBY:
		return sizeof(game_message_payload_request_join_lobby);

	case GAME_MESSAGE_TYPE::JOIN_LOBBY_RESPONSE:
		return sizeof(game_message_payload_join_lobby_response);

	case GAME_MESSAGE_TYPE::SIGNAL_LEAVE_LOBBY:
		return sizeof(game_message_payload_signal_leave_lobby);

	case GAME_MESSAGE_TYPE::LOBBY_CLIENT_KICKED:
		return sizeof(game_message_payload_lobby_client_kicked);

	case GAME_MESSAGE_TYPE::LOBBY_ADMIN_START_MATCH:
		return sizeof(game_message_payload_lobby_admin_start_match);

	case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_PLAYER_COUNT:
		return sizeof(game_message_payload_lobby_admin_set_player_count);

	case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_MAP_SIZE:
		return sizeof(game_message_payload_lobby_admin_set_map_size);

	case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_SEED:
		return sizeof(game_message_payload_lobby_admin_set_seed);

	default:
		ASSERT_MSG(0, "Message type %d is missing a payload structure size association.", type);
		return 0;
	}
}

#ifndef NDEBUG

// Simple testing function to ensure all declared message types have an associated size,
// and by extension, a payload structure.
static void TEST_MESSAGE_TYPE_SIZES_CHECK()
{
	for (ui8 gameMessageIndex = 0; gameMessageIndex < (ui8)GAME_MESSAGE_TYPE::TYPE_COUNT; gameMessageIndex++)
	{
		ASSERT(get_message_type_payload_size((GAME_MESSAGE_TYPE)gameMessageIndex));
	}
}

#endif

// Builds a message of specified type in provided memory arena.
// Additional memory can be allocated along with the standard payload size with extra_payload_size.
// Returns pointer to header if successful.
static game_message_header* build_game_message(GAME_MESSAGE_TYPE type, mem_arena& target_mem, ui16 extra_payload_size = 0)
{
	game_message_header* header = 
		(game_message_header*)target_mem.alloc(sizeof(game_message_header) 
												+ get_message_type_payload_size(type) + extra_payload_size, 1); // Combined alloc of header & payload.
	if (header == nullptr) return nullptr;
	*header = {};

	header->message_type = type;
	header->payload_size = get_message_type_payload_size(type) + extra_payload_size;

	return header;
}

#pragma pack(pop)

#endif // GAME_MESSAGES_INCLUDED
