// Net message handling for the client backend.

#include "core.h"

#include "game_client.h"

#include "game_common/match/match.h"
#include "game_common/game_messages.h"

ui8* game_client_get_net_msg_buffer(game_client& backend)
{
	return backend.net_msg_buffer;
}

ui32 game_client_get_net_msg_buffer_size(game_client& backend)
{
	return backend.NET_MSG_BUFFER_SIZE;
}

ui8* game_client_get_net_msg_output(game_client& backend)
{
	return backend.net_output_buffer;
}

ui32 game_client_get_net_msg_output_size(game_client& backend)
{
	return backend.net_output_msg_size;
}

// Fills in the provided buffer with the CLIENT_TICK message the client should send, if any, draining the input
// command queue into it.
void game_client_output_client_tick_message(game_client& backend)
{
	backend.net_output_msg_size = 0;

	match_command_sequence& queuedSequence = *backend.input.command_queue_builder._sequence_start;
	if (queuedSequence.command_count == 0) return;

	// Total bytes to copy: the sequence's fixed part plus its raw <header><payload>... commands buffer.
	ui32 queuedSequenceBytes = (ui32)backend.input.command_queue.allocated_count;

	// Extra bytes to allocate beyond game_message_payload_client_tick's own size: just the commands buffer, since
	// the sequence's fixed part is already accounted for by the embedded commands member.
	ui32 queuedCommandBytes = queuedSequenceBytes - sizeof(match_command_sequence);

	mem_arena outgoing_mem = mem_arena_create(backend.net_output_buffer, game_client::NET_OUTPUT_BUFFER_SIZE);

	game_message_header* header = build_game_message(GAME_MESSAGE_TYPE::CLIENT_TICK, outgoing_mem, queuedCommandBytes);
	ASSERT(header != nullptr);

	auto& payload = header->get_payload_ref<game_message_payload_client_tick>();
	payload.emit_tick = backend.local_match->tick;

	ia_memcpy(&payload.commands, &queuedSequence, queuedSequenceBytes);

	backend.net_output_msg_size = sizeof(game_message_header) + header->payloadSize;

	// Reset the queue: clear the arena and re-init the builder so the next input event has a fresh sequence ready.
	backend.input.command_queue.clear();
	ASSERT(backend.input.command_queue_builder.init());
}

GAME_MESSAGE_TYPE game_client_process_game_message(game_client& backend, ui32 message_size)
{
	if (message_size < sizeof(game_message_header)) return GAME_MESSAGE_TYPE::INVALID;

	game_message_header* header = (game_message_header*)backend.net_msg_buffer;
	if (message_size < sizeof(game_message_header) + header->payloadSize) return GAME_MESSAGE_TYPE::INVALID;

	switch (header->message_type)
	{
	// Signal from the server that a match was just joined by the client. Use the provided parameters to start the local match.
	case GAME_MESSAGE_TYPE::MATCH_JOINED:
	{
		auto& payload = header->get_payload_ref<game_message_payload_match_joined>();

		backend.controlled_player_id = payload.controlled_player_id;

		// Copy the params (+ their trailing extra data) into the match's own memory: the net message buffer is reused for the next message.
		backend.local_match_mem.clear();

		ui32 paramsBlockSize = sizeof(game_match_start_params) + payload.match_start_params.extra_data_size;
		ui8* paramsCopy = backend.local_match_mem.alloc<ui8>(paramsBlockSize);
		ASSERT(paramsCopy != nullptr);
		ia_memcpy(paramsCopy, &payload.match_start_params, paramsBlockSize);

		if (!game_client_begin_match_with_params(backend, *(game_match_start_params*)paramsCopy, paramsBlockSize)) return GAME_MESSAGE_TYPE::INVALID;

		break;
	}
	// Input commands from the server, for the client to simulate the next match tick with.
	case GAME_MESSAGE_TYPE::SERVER_TICK:
	{
		if (backend.local_match == nullptr) return GAME_MESSAGE_TYPE::INVALID;

		game_match& localMatch = *backend.local_match;
		auto& payload = header->get_payload_ref<game_message_payload_server_tick>();

		// Tick local match on receiving server tick data.
		match_tick(localMatch, payload.commands);
		game_client_output_client_tick_message(backend);
		break;
	}
	default:
		return GAME_MESSAGE_TYPE::INVALID;
	}

	return header->message_type;
}
