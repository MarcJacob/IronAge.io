// Net message handling for the client backend.

#include "game_client_backend.h"

// Builds a CLIENT_TICK message out of the pending input, if any, into backend.net_output_buffer.
// Resets backend.net_output_msg_size to 0 first: a message is only ever offered once, right after it's built.
void client_backend_build_net_output_message(client_backend& backend)
{
	backend.net_output_msg_size = 0;

	if (!backend.input.pending) return;

	mem_arena outgoing_mem = mem_arena_create(backend.net_output_buffer, client_backend::NET_OUTPUT_BUFFER_SIZE);

	game_message_header* header = build_game_message(GAME_MESSAGE_TYPE::CLIENT_TICK, outgoing_mem,
		sizeof(match_command_header) + sizeof(command_payload_set_entity_move_target));
	ASSERT(header != nullptr);

	auto& payload = header->get_payload_ref<game_message_payload_client_tick>();
	payload.emit_tick = client_backend_get_local_match(backend).tick;
	payload.command_header_count = 1;

	match_command_header* commandHeader = (match_command_header*)payload._command_headers_buffer;
	*commandHeader = {};
	commandHeader->type = MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET;
#ifdef MATCH_COMMAND_DEBUG
	commandHeader->_data_size = sizeof(command_payload_set_entity_move_target);
#endif

	command_payload_set_entity_move_target& movePayload = commandHeader->get_command_payload<command_payload_set_entity_move_target>();
	movePayload.target_entity = backend.controlled_player_id;
	movePayload.new_target = { (ui16)backend.input.target_loc_x, (ui16)backend.input.target_loc_y };

	backend.net_output_msg_size = sizeof(game_message_header) + header->payloadSize;
	backend.input.pending = false;
}

GAME_MESSAGE_TYPE client_backend_process_net_message(client_backend& backend, ui32 message_size)
{
	if (message_size < sizeof(game_message_header)) return GAME_MESSAGE_TYPE::TYPE_COUNT;

	game_message_header* header = (game_message_header*)backend.net_msg_buffer;
	if (message_size < sizeof(game_message_header) + header->payloadSize) return GAME_MESSAGE_TYPE::TYPE_COUNT;

	switch (header->message_type)
	{
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

		if (!client_backend_begin_match_with_params(backend, *(game_match_start_params*)paramsCopy)) return GAME_MESSAGE_TYPE::TYPE_COUNT;

		break;
	}
	case GAME_MESSAGE_TYPE::SERVER_TICK:
	{
		if (backend.local_match == nullptr) return GAME_MESSAGE_TYPE::TYPE_COUNT;

		auto& payload = header->get_payload_ref<game_message_payload_server_tick>();
		match_tick(client_backend_get_local_match(backend), payload.commands);

		client_backend_rebuild_render_state(backend);
		client_backend_build_net_output_message(backend);

		break;
	}
	default:
		return GAME_MESSAGE_TYPE::INVALID;
	}

	return header->message_type;
}
