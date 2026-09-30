// Symbol declaration for Game Message system, network-oriented message structures.

#ifndef GAME_MESSAGES_INCLUDED
#define GAME_MESSAGES_INCLUDED

#include "match/commands.h" // For match_tick_commands, embedded by value in the tick message payloads below.

// Enumaration of supported game message types.
// Each type features a high-level description of its functionality. More detail can be found atop the corresponding payload structure.
enum class GAME_MESSAGE_TYPE : ui8
{
	MATCH_JOINED,		// Server -> Client = Contains instructions on the match the client is joining, including which player is controlled.
	MATCH_ENDED,		// Server -> Client = Signal of match ending on the server, with result information.

	SERVER_TICK,		// Server -> Client = Input sequence for a specific tick provided by the server to the client. Allows client to progress local match state.
	SERVER_TICK_BUNDLE, // Server -> Client = Multiple ticks worth of input sequences. 
	CLIENT_TICK,

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

	ui16 payloadSize;
	ui8 _payload[];

	// Get a typed reference to the beginning of the payload bytes.
	// Asserts that the payload is at least large enough for the type of payload desired, but does NOT guarantee anything beyond that,
	// specifically the validity of the values or the coherence of the size of the payload for dynamically-sized messages.
	template<typename PayloadType>
	inline PayloadType& get_payload_ref() { ASSERT(payloadSize >= sizeof(PayloadType)); return *(PayloadType*)_payload; }
};

// BEGIN GAME SERVER CORE MESSAGES

// Server -> Client message payload.
// Received by a client as instruction by the server of what the match starting / current state is, and what player ID they are in control of.
struct game_message_payload_match_joined
{
	ui32 join_tick; // What tick the match is currently on as the client joins. Currently this means "You will have to catch up to this tick" if above 0.
					// TODO: Allow this message (or an alternative form of it) to feature a snapshot of the game state to speedup mid-game joining process ?

	match_player_id controlled_player_id; // What player ID the client is in charge of controlling.

	game_match_start_params match_start_params; // Match start parameters. Don't forget it features post-structure bytes for dynamic world state elements !
};

// Server -> Client message payload.
// Received by a client as signal that the match they're in has ended and that the server will supply no further tick messages.
// Currently this is also a signal that 
struct game_message_payload_match_ended
{
	match_player_id winner_id; // ID of the victorious player.
};

// END GAME SERVER CORE MESSAGES

// BEGIN MATCH COMMAND MESSAGES

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
	case GAME_MESSAGE_TYPE::MATCH_JOINED:
		return sizeof(game_message_payload_match_joined);
	case GAME_MESSAGE_TYPE::MATCH_ENDED:
		return sizeof(game_message_payload_match_ended);
	case GAME_MESSAGE_TYPE::SERVER_TICK:
		return sizeof(game_message_payload_server_tick);
	case GAME_MESSAGE_TYPE::SERVER_TICK_BUNDLE:
		return sizeof(game_message_payload_server_tick_bundle);
	case GAME_MESSAGE_TYPE::CLIENT_TICK:
		return sizeof(game_message_payload_client_tick);
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
	header->payloadSize = get_message_type_payload_size(type) + extra_payload_size;

	return header;
}

#pragma pack(pop)

#endif // GAME_MESSAGES_INCLUDED
