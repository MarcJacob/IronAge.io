// Symbol declarations for managing a match slot 
// during its IN_PLAY and PLAY_ENDED state.

#ifndef MATCH_IN_PLAY_INCLUDED
#define MATCH_IN_PLAY_INCLUDED

#include "core.h"
#include "match_slots.h"

// Handler for CLIENT_PLACEMENT_PICK message.
void match_slot_handle_message_client_placement_pick(game_server& server, match_slot_index slot_index, game_server_client* player_client, const game_message_header& msg);

// Handler for CLIENT_TICK game message.
void match_slot_handle_message_client_tick(game_server& server, match_slot_index slot_index, game_server_client* player_client, const game_message_header& msg);

// Tick logic for a slot in ONGOING state.
void game_server_tick_match_slot_ongoing(game_server& server, ui8 slot_index);


#endif // MATCH_IN_PLAY_INCLUDED
