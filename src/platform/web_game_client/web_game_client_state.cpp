// Client state exports: current state, lobby data, and the requests JS sends (authentication, lobbies, lobby admin).
// Data is read by backend.ts at fixed offsets (game_messages.h structures, packed).
// Requests are built in the backend's request buffer: each export returns the message size (0 = not allowed in the current state),
// and JS sends that many bytes from client_get_net_request_buffer_offset right away.

#include "core.h"
#include "web_game_client.h"

WASM_EXPORT CLIENT_STATE client_get_state()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return WEB_CLIENT.backend->state.current;
}

// player_name kept by the server (empty before AUTHENTICATED).
WASM_EXPORT player_name* client_get_own_name_offset()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return &WEB_CLIENT.backend->state.own_name;
}

// Name of a player of the current match: pointer to its characters (not zero-terminated, see client_get_player_name_length) and length.
// Length 0 for an out of range ID (neutral included). Valid until the next match starts.
WASM_EXPORT const char* client_get_player_name_offset(ui32 player_index)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_get_player_name(*WEB_CLIENT.backend, (match_player_id)player_index).view_str;
}

WASM_EXPORT ui32 client_get_player_name_length(ui32 player_index)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_get_player_name(*WEB_CLIENT.backend, (match_player_id)player_index).length;
}

// game_message_payload_server_lobby_list: last lobby list received.
WASM_EXPORT game_message_payload_server_lobby_list* client_get_lobby_list_offset()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return &WEB_CLIENT.backend->state.lobby_list;
}

// game_lobby_state: state of the lobby the client is in (zeroed outside of a lobby).
WASM_EXPORT game_lobby_state* client_get_lobby_state_offset()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return &WEB_CLIENT.backend->state.lobby_state;
}

// game_message_payload_join_lobby_response: last join response received.
WASM_EXPORT game_message_payload_join_lobby_response* client_get_join_response_offset()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return &WEB_CLIENT.backend->state.join_response;
}

// Whether the client was kicked from its last lobby (cleared by the next join request).
WASM_EXPORT bool client_was_kicked()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return WEB_CLIENT.backend->state.kicked;
}

// LOBBY_KICK_REASON of the last kick. Only meaningful when client_was_kicked.
WASM_EXPORT LOBBY_KICK_REASON client_get_kick_reason()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return WEB_CLIENT.backend->state.kick_reason;
}

// REQUESTS

WASM_EXPORT ui8* client_get_net_request_buffer_offset()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_get_net_request_buffer(*WEB_CLIENT.backend);
}

// The name is the first name_length bytes (UTF-8) JS wrote in the input event buffer (web_game_client_input.cpp). Cut to PLAYER_NAME_MAX_LENGTH.
WASM_EXPORT ui32 client_request_authentication(ui32 name_length)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	ia_string_view name((const char*)INPUT_EVENT_BUFFER, ia_min(name_length, (ui32)PLAYER_NAME_MAX_LENGTH));
	return game_client_request_authentication(*WEB_CLIENT.backend, name);
}

WASM_EXPORT ui32 client_request_join_lobby(ui8 lobby_id)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_request_join_lobby(*WEB_CLIENT.backend, lobby_id);
}

WASM_EXPORT ui32 client_signal_leave_lobby()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_signal_leave_lobby(*WEB_CLIENT.backend);
}

WASM_EXPORT ui32 client_lobby_admin_start_match()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_lobby_admin_start_match(*WEB_CLIENT.backend);
}

WASM_EXPORT ui32 client_lobby_admin_set_player_count(ui16 player_count)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_lobby_admin_set_player_count(*WEB_CLIENT.backend, player_count);
}

WASM_EXPORT ui32 client_lobby_admin_set_map_size(ui16 width_regions, ui16 height_regions)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_lobby_admin_set_map_size(*WEB_CLIENT.backend, width_regions, height_regions);
}

WASM_EXPORT ui32 client_lobby_admin_set_seed(ui32 random_seed)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_lobby_admin_set_seed(*WEB_CLIENT.backend, random_seed);
}

WASM_EXPORT ui32 client_lobby_admin_set_tick_rate(ui8 tick_rate)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_lobby_admin_set_tick_rate(*WEB_CLIENT.backend, tick_rate);
}
