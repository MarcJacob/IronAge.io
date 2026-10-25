// Symbol declarations for managing a match slot during its IN_LOBBY state.

#ifndef MATCH_SLOT_LOBBY_INCLUDED
#define MATCH_SLOT_LOBBY_INCLUDED

#include "core.h"
#include "match_slots.h"

// Tick logic for a slot in LOBBY state.
void game_server_tick_match_slot_lobby(game_server& server, ui8 slot_index);


#endif // MATCH_SLOT_LOBBY_INCLUDED
