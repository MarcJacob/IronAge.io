// Implementation file for Lobby state of game server match slots.

#include "core.h"
#include "match_slots.h"
#include "../game_server.h"

bool game_server_open_lobby(game_server& server, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	
	if (slot.state != MATCH_SLOT_STATE::WAITING)
	{
		server.logf("MATCH", LOG_ERROR, "Attempted to open lobby in slot %d which wasn't properly (re)initialized.", slot_index);
		return false;
	}

	server.logf("MATCH", LOG_NORMAL, "Opening lobby in match slot %d.", slot_index);

	slot.state = MATCH_SLOT_STATE::IN_LOBBY;
	slot.players = slot.slot_memory.alloc<match_player>(slot.match_params->player_count);
	ASSERT(slot.players != nullptr);

    slot.lobby = slot.slot_memory.alloc<match_slot_lobby>();
    slot.lobby->alloc_start = slot.slot_memory.allocated_count;

    slot.lobby->countdown_active = false;
    slot.lobby->countdown_end_ms = server.uptime_ms + LOBBY_COUNTDOWN_MS;

	return true;
}

// Sends the lobby's state (countdown, player count, slot index, match params) to every client in it.
static void game_server_slot_send_lobby_state(game_server& server, match_slot_index slot_index)
{
    match_slot& slot = server.match_slots[slot_index];

	ui8 msgBuff[64];
	mem_arena msgMem = mem_arena_create(msgBuff, sizeof(msgBuff));

	game_message_header* msgHeader = build_game_message(GAME_MESSAGE_TYPE::LOBBY_JOINED, msgMem);
	ASSERT(msgHeader);

	auto& payload = msgHeader->get_payload_ref<game_message_payload_lobby_joined>();
	payload.countdown_remaining_ms = (slot.lobby->countdown_active && slot.lobby->countdown_end_ms > server.uptime_ms) ? (ui32)(slot.lobby->countdown_end_ms - server.uptime_ms) : 0;
	payload.lobby_player_count = slot.connection_count;
	payload.match_slot_index = slot_index;
	ia_memcpy(&payload.match_start_params, slot.match_params, sizeof(game_match_start_params));

	game_server_slot_broadcast(server, slot, *msgHeader);
}

// Ticks a match slot that is currently in its lobby, waiting for players. Starts the match once the countdown ends, with whoever is present.
void game_server_tick_match_slot_lobby(game_server& server, ui8 slot_index)
{
    match_slot& slot = server.match_slots[slot_index];

	// All player indices filled: no reason to wait for the countdown.
	if (slot.connection_count >= slot.match_params->player_count)
	{
		server.logf("MATCH", LOG_NORMAL, "Lobby on slot %d is full. Starting match immediately.", slot_index);
	}
    // The first client to join starts the countdown.
    else if (slot.connection_count > 0 && !slot.lobby->countdown_active)
	{
		slot.lobby->countdown_active = true;
		slot.lobby->countdown_end_ms = server.uptime_ms + LOBBY_COUNTDOWN_MS;
        game_server_slot_send_lobby_state(server, slot_index);
        return;
	}
    // Reset countdown if it started and player count went back to 0.
    else if (slot.connection_count == 0 && slot.lobby->countdown_active)
    {
        slot.lobby->countdown_active = false;
        return;
    }
    // In other situations, just wait until the countdown is passed.
    else if (!slot.lobby->countdown_active || server.uptime_ms < slot.lobby->countdown_end_ms) 
    {
        // Every second, broadcast current lobby state.
        if (server.uptime_ms > slot.lobby->lobby_state_refresh_time)
        {
            slot.lobby->lobby_state_refresh_time = server.uptime_ms + 1000;
            game_server_slot_send_lobby_state(server, slot_index);
        }
        return;
    }

	// On failure the slot goes to AWAITING_CLEANUP and gets reset.
	if (!game_server_start_match_slot(server, slot_index))
    {
        server.logf("MATCH", LOG_ERROR, "Failed to start match on slot %d !", slot_index);
    }
}
