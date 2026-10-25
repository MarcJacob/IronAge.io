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

	backend.net_output_msg_size = sizeof(game_message_header) + header->payload_size;

	// Reset the queue: clear the arena and re-init the builder so the next input event has a fresh sequence ready.
	backend.input.command_queue.clear();
	ASSERT(backend.input.command_queue_builder.init());
}

// CLIENT STATE REQUESTS

ui8* game_client_get_net_request_buffer(game_client& backend)
{
	return backend.net_request_buffer;
}

// Builds a message of the given type in the request buffer. Returns its payload, to be filled in.
template<typename PayloadType>
static PayloadType& game_client_build_request(game_client& backend, GAME_MESSAGE_TYPE type)
{
	static_assert(sizeof(game_message_header) + sizeof(PayloadType) <= game_client::NET_REQUEST_BUFFER_SIZE, "Request does not fit the request buffer.");

	ia_memzero(backend.net_request_buffer, game_client::NET_REQUEST_BUFFER_SIZE); // build_game_message does not zero the payload.
	mem_arena requestMem = mem_arena_create(backend.net_request_buffer, game_client::NET_REQUEST_BUFFER_SIZE);
	game_message_header* header = build_game_message(type, requestMem);
	ASSERT(header != nullptr);
	return header->get_payload_ref<PayloadType>();
}

static ui32 game_client_request_size(game_client& backend)
{
	return sizeof(game_message_header) + ((game_message_header*)backend.net_request_buffer)->payload_size;
}

ui32 game_client_request_authentication(game_client& backend, const ia_string_view& name)
{
	if (backend.state.current != CLIENT_STATE::CONNECTING) return 0;

	auto& payload = game_client_build_request<game_message_payload_request_authentication>(backend, GAME_MESSAGE_TYPE::REQUEST_AUTHENTICATION);
	payload.name = ia_string_view(name.view_str, ia_min(name.length, (ui32)PLAYER_NAME_MAX_LENGTH));

	backend.state.current = CLIENT_STATE::AUTHENTICATING;
	return game_client_request_size(backend);
}

ui32 game_client_request_join_lobby(game_client& backend, ui8 lobby_id)
{
	if (backend.state.current != CLIENT_STATE::LOBBY_LIST) return 0;

	game_client_build_request<game_message_payload_request_join_lobby>(backend, GAME_MESSAGE_TYPE::REQUEST_JOIN_LOBBY).lobby_id = lobby_id;

	backend.state.kicked = false;
	backend.state.join_response = {};
	backend.state.current = CLIENT_STATE::JOINING_LOBBY;
	return game_client_request_size(backend);
}

ui32 game_client_signal_leave_lobby(game_client& backend)
{
	if (backend.state.current != CLIENT_STATE::IN_LOBBY) return 0;

	game_client_build_request<game_message_payload_signal_leave_lobby>(backend, GAME_MESSAGE_TYPE::SIGNAL_LEAVE_LOBBY);

	backend.state.lobby_state = {};
	backend.state.current = CLIENT_STATE::LOBBY_LIST;
	return game_client_request_size(backend);
}

// Whether lobby admin requests can be sent: in a test lobby.
static bool game_client_can_send_lobby_admin(game_client& backend)
{
	return backend.state.current == CLIENT_STATE::IN_LOBBY && backend.state.lobby_state.test_mode;
}

ui32 game_client_lobby_admin_start_match(game_client& backend)
{
	if (!game_client_can_send_lobby_admin(backend)) return 0;
	game_client_build_request<game_message_payload_lobby_admin_start_match>(backend, GAME_MESSAGE_TYPE::LOBBY_ADMIN_START_MATCH);
	return game_client_request_size(backend);
}

ui32 game_client_lobby_admin_set_player_count(game_client& backend, ui16 player_count)
{
	if (!game_client_can_send_lobby_admin(backend)) return 0;
	game_client_build_request<game_message_payload_lobby_admin_set_player_count>(backend, GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_PLAYER_COUNT).player_count = player_count;
	return game_client_request_size(backend);
}

ui32 game_client_lobby_admin_set_map_size(game_client& backend, ui16 width_regions, ui16 height_regions)
{
	if (!game_client_can_send_lobby_admin(backend)) return 0;
	game_client_build_request<game_message_payload_lobby_admin_set_map_size>(backend, GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_MAP_SIZE).world_size_regions = { width_regions, height_regions };
	return game_client_request_size(backend);
}

ui32 game_client_lobby_admin_set_seed(game_client& backend, ui32 random_seed)
{
	if (!game_client_can_send_lobby_admin(backend)) return 0;
	game_client_build_request<game_message_payload_lobby_admin_set_seed>(backend, GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_SEED).random_seed = random_seed;
	return game_client_request_size(backend);
}

GAME_MESSAGE_TYPE game_client_process_game_message(game_client& backend, ui32 message_size)
{
	if (message_size < sizeof(game_message_header)) return GAME_MESSAGE_TYPE::INVALID;

	game_message_header* header = (game_message_header*)backend.net_msg_buffer;
	if (message_size < sizeof(game_message_header) + header->payload_size) return GAME_MESSAGE_TYPE::INVALID;

	// Unknown types and payloads smaller than their structure are not processed.
	if (header->message_type >= GAME_MESSAGE_TYPE::TYPE_COUNT || header->payload_size < get_message_type_payload_size(header->message_type)) return GAME_MESSAGE_TYPE::INVALID;

	switch (header->message_type)
	{
	// CLIENT STATE (connection flow)

	case GAME_MESSAGE_TYPE::AUTHENTICATED:
	{
		if (backend.state.current != CLIENT_STATE::AUTHENTICATING) return GAME_MESSAGE_TYPE::INVALID;

		const player_name& name = header->get_payload_ref<game_message_payload_authenticated>().name;
		if (name.length > PLAYER_NAME_MAX_LENGTH) return GAME_MESSAGE_TYPE::INVALID;
		backend.state.own_name = (ia_string_view)name;

		backend.state.current = CLIENT_STATE::LOBBY_LIST;
		break;
	}
	case GAME_MESSAGE_TYPE::SERVER_LOBBY_LIST:
	{
		auto& payload = header->get_payload_ref<game_message_payload_server_lobby_list>();
		backend.state.lobby_list = payload;
		if (backend.state.lobby_list.lobby_count > LOBBY_LIST_MAX_COUNT) backend.state.lobby_list.lobby_count = LOBBY_LIST_MAX_COUNT;
		break;
	}
	case GAME_MESSAGE_TYPE::JOIN_LOBBY_RESPONSE:
	{
		if (backend.state.current != CLIENT_STATE::JOINING_LOBBY) return GAME_MESSAGE_TYPE::INVALID;

		backend.state.join_response = header->get_payload_ref<game_message_payload_join_lobby_response>();
		backend.state.current = (backend.state.join_response.result == JOIN_LOBBY_RESULT::SUCCESS) ? CLIENT_STATE::IN_LOBBY : CLIENT_STATE::LOBBY_LIST;
		break;
	}
	// Lobby state, while in the lobby.
	case GAME_MESSAGE_TYPE::LOBBY_JOINED:
	{
		if (backend.state.current != CLIENT_STATE::IN_LOBBY) return GAME_MESSAGE_TYPE::INVALID;

		backend.state.lobby_state = header->get_payload_ref<game_message_payload_lobby_joined>().lobby;
		break;
	}
	case GAME_MESSAGE_TYPE::LOBBY_CLIENT_KICKED:
	{
		if (backend.state.current != CLIENT_STATE::IN_LOBBY) return GAME_MESSAGE_TYPE::INVALID;

		backend.state.kicked = true;
		backend.state.kick_reason = header->get_payload_ref<game_message_payload_lobby_client_kicked>().reason;
		backend.state.lobby_state = {};
		backend.state.current = CLIENT_STATE::LOBBY_LIST;
		break;
	}

	// MATCH

	// Signal from the server that a match was just joined by the client. Use the provided parameters to start the local match.
	case GAME_MESSAGE_TYPE::MATCH_STARTED:
	{
		if (backend.state.current != CLIENT_STATE::IN_LOBBY) return GAME_MESSAGE_TYPE::INVALID;

		auto& payload = header->get_payload_ref<game_message_payload_match_started>();

		// The player names list follows the payload structure: one name per player, all of a valid length.
		ui16 namesCount = payload.match_start_params.player_count;
		if (namesCount > LOBBY_MAX_PLAYER_COUNT) return GAME_MESSAGE_TYPE::INVALID;
		if (header->payload_size < sizeof(game_message_payload_match_started) + namesCount * sizeof(player_name)) return GAME_MESSAGE_TYPE::INVALID;

		const player_name* names = (const player_name*)((const ui8*)&payload + sizeof(game_message_payload_match_started));
		for (ui16 nameIndex = 0; nameIndex < namesCount; nameIndex++)
		{
			if (names[nameIndex].length > PLAYER_NAME_MAX_LENGTH) return GAME_MESSAGE_TYPE::INVALID;
		}

		backend.state.current = CLIENT_STATE::IN_MATCH;

		// Replaces the previous match's names.
		backend.state.match_player_name_count = namesCount;
		for (ui16 nameIndex = 0; nameIndex < namesCount; nameIndex++)
		{
			backend.state.match_player_names[nameIndex] = (ia_string_view)names[nameIndex];
		}

		backend.controlled_player_id = payload.controlled_player_id;

		// Copy the params (+ their trailing extra data) into the match's own memory: the net message buffer is reused for the next message.
		backend.local_match_mem.clear();

		ui32 paramsBlockSize = sizeof(game_match_start_params);
		ui8* paramsCopy = backend.local_match_mem.alloc<ui8>(paramsBlockSize);
		ASSERT(paramsCopy != nullptr);
		ia_memcpy(paramsCopy, &payload.match_start_params, paramsBlockSize);

		if (!game_client_begin_match_with_params(backend, *(game_match_start_params*)paramsCopy, paramsBlockSize)) return GAME_MESSAGE_TYPE::INVALID;

		// Refresh render state so it can take match params into account.
		game_client_rebuild_render_state(backend.render_state, *backend.local_match);

		break;
	}
	// The frontend reads placement picks straight from the net message buffer: nothing to do here.
	case GAME_MESSAGE_TYPE::SERVER_PLACEMENT_PICK:
		break;
	// The frontend reads the winner from the buffer, then calls game_client_end_local_match
	// (through the platform) to drop the local match. Cleanup lives in that one place only.
	// The server takes the client out of the lobby: it gets the lobby list again.
	case GAME_MESSAGE_TYPE::MATCH_ENDED:
		if (backend.state.current != CLIENT_STATE::IN_MATCH) return GAME_MESSAGE_TYPE::INVALID;
		backend.state.lobby_state = {};
		backend.state.current = CLIENT_STATE::LOBBY_LIST;
		break;
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
