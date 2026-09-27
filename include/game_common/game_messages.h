// Symbol declaration for Game Message system, network-oriented message structures.

#ifndef GAME_MESSAGES_INCLUDED
#define GAME_MESSAGES_INCLUDED

// Enumaration of supported game message types.
// Each type features a high-level description of its functionality. More detail can be found atop the corresponding payload structure.
enum class GAME_MESSAGE_TYPE
{
	JOIN_MATCH,			// Server -> Client = Contains instructions on the match the client is joining, including which player is controlled.
	END_MATCH,			// Server -> Client = Signal of match ending on the server, with result information.

	SERVER_TICK,		// Server -> Client = Input sequence for a specific tick provided by the server to the client. Allows client to progress local match state.
	SERVER_TICK_BUNDLE, // Server -> Client = Multiple ticks worth of input sequences. 
	CLIENT_TICK,
};

// Defines the first portion of a game message.
// Use to interpret the following payload memory to the correct structure.
struct game_message_header
{
	ui16 message_type_code;

	ui16 payloadSize;
	ui8 _payload[];

	// Get a typed reference to the beginning of the payload bytes.
	// Asserts that the payload is at least large enough for the type of payload desired, but does NOT guarantee anything beyond that,
	// specifically the validity of the values or the coherence of the size of the payload for dynamically-sized messages.
	template<typename PayloadType>
	inline PayloadType& ReadPayload() { ASSERT(payloadSize >= sizeof(PayloadType)); return *(PayloadType*)_payload; }
};

// BEGIN GAME SERVER CORE MESSAGES

// Server -> Client message payload.
// Received by a client as instruction by the server of what the match starting / current state is, and what player ID they are in control of.
struct game_message_payload_join_match
{
	ui32 join_tick; // What tick the match is currently on as the client joins. Currently this means "You will have to catch up to this tick" if above 0.
					// TODO: Allow this message (or an alternative form of it) to feature a snapshot of the game state to speedup mid-game joining process ?

	match_player_id controlled_player_id; // What player ID the client is in charge of controlling.

	game_match_start_params match_start_params; // Match start parameters. Don't forget it features post-structure bytes for dynamic world state elements !
};

// Server -> Client message payload.
// Received by a client as signal that the match they're in has ended and that the server will supply no further tick messages.
// Currently this is also a signal that 
struct game_message_payload_match_end
{
	match_player_id winner_id; // ID of the victorious player.
};

// END GAME SERVER CORE MESSAGES

// BEGIN MATCH COMMAND MESSAGES

struct match_command_header;
struct match_command_sequence;
struct match_tick_commands;

// Server -> Client message payload.
// Received by a client as the authoritative set of input command sequences applied over a specific match tick.
struct game_message_payload_server_tick
{
	ui32 apply_tick; // Which tick the input sequence happened for.

	// Buffer of command sequences following this structure.
	// Contains command sequences
	ui16 command_sequence_count;
	ui8 _command_sequences_buffer[];

	// Returns a typed reference of a match command sequence start struct at the provided byte offset.
	inline match_command_sequence& get_sequence_at(ui16 byte_offset) const
	{
		return *(match_command_sequence*)&_command_sequences_buffer[byte_offset];
	}
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

	// Buffer of commands immediately following this structure.
	// Directly contains command headers with their payloads.
	ui8 command_header_count;
	ui8 _command_headers_buffer[];

	// Returns a typed reference of a match command sequence start struct at the provided byte offset.
	inline match_command_header& get_command_at(ui16 byte_offset) const
	{
		return *(match_command_header*)&_command_headers_buffer[byte_offset];
	}
};

// END MATCH COMMAND MESSAGES

#endif // GAME_MESSAGES_INCLUDED
