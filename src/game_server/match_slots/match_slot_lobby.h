// Symbol declarations for managing a match slot during its IN_LOBBY state.

#ifndef MATCH_SLOT_LOBBY_INCLUDED
#define MATCH_SLOT_LOBBY_INCLUDED

#include "core.h"
#include "match_slots.h"

// Tick logic for a slot in LOBBY state.
void game_server_tick_match_slot_lobby(game_server& server, ui8 slot_index);

// Helper functions to control the lobby from outside.

// Fills the lobby state of a slot in lobby, as clients see it.
void game_server_slot_fill_lobby_state(game_server& server, match_slot_index slot_index, game_lobby_state& out_state);

// Sends the lobby's state (LOBBY_JOINED) to every client in it.
void game_server_slot_send_lobby_state(game_server& server, match_slot_index slot_index);

// Fills a lobby list with every slot currently in lobby.
void game_server_fill_lobby_list(game_server& server, game_message_payload_server_lobby_list& out_list);

// Handler for REQUEST_JOIN_LOBBY, from an authenticated client. Always answers with JOIN_LOBBY_RESPONSE.
void game_server_lobby_handle_join_request(game_server& server, game_server_client& client, const game_message_header& msg);

// Handler for SIGNAL_LEAVE_LOBBY, from a client in the slot. Detaches it whatever the slot state (in a match, as on a disconnection).
void match_slot_handle_message_leave_lobby(game_server& server, match_slot_index slot_index, game_server_client* player_client);

// Handler for LOBBY_ADMIN_* messages, from a client in the slot. Ignored unless the slot is a test lobby.
void match_slot_handle_message_lobby_admin(game_server& server, match_slot_index slot_index, game_server_client* player_client, const game_message_header& msg);


#endif // MATCH_SLOT_LOBBY_INCLUDED
