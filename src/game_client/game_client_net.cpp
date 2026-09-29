// Net message handling for the client backend.

#include "core.h"

#include "game_client.h"

#include "game_common/game_match.h"
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

// Fills in the provided buffer with the CLIENT_TICK message the client should send, if any.
void game_client_output_client_tick_message(game_client& backend)
{
	backend.net_output_msg_size = 0;

	if (!backend.input.pending) return;

	mem_arena outgoing_mem = mem_arena_create(backend.net_output_buffer, game_client::NET_OUTPUT_BUFFER_SIZE);

	game_message_header* header = build_game_message(GAME_MESSAGE_TYPE::CLIENT_TICK, outgoing_mem,
		sizeof(match_command_header) + sizeof(command_payload_set_entity_move_target));
	ASSERT(header != nullptr);

	auto& payload = header->get_payload_ref<game_message_payload_client_tick>();
	payload.emit_tick = backend.local_match->tick;
	payload.command_header_count = 1;

	match_command_header* commandHeader = (match_command_header*)payload._command_headers_buffer;
	*commandHeader = {};
	commandHeader->type = MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET;
#ifdef MATCH_COMMAND_DEBUG
	commandHeader->_data_size = sizeof(command_payload_set_entity_move_target);
#endif

	command_payload_set_entity_move_target& movePayload = commandHeader->get_command_payload<command_payload_set_entity_move_target>();
	movePayload.target_entity = backend.controlled_player_id;
	movePayload.new_target = { backend.input.pending_target.x, backend.input.pending_target.y };

	backend.net_output_msg_size = sizeof(game_message_header) + header->payloadSize;

	backend.input.pending = false;
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
