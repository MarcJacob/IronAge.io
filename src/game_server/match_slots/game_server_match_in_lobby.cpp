// Implementation file for Lobby state of game server match slots.

#include "core.h"
#include "match_slots.h"
#include "match_slot_lobby.h"

#include "../game_server.h"

bool game_server_match_slot_open_lobby(game_server& server, ui8 slot_index)
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

	// Room for the hard maximum of players, so the player cap can change during the lobby.
	ASSERT(slot.match_params->player_count <= LOBBY_MAX_PLAYER_COUNT);
	slot.players = slot.slot_memory.alloc<match_player>(LOBBY_MAX_PLAYER_COUNT);
	ASSERT(slot.players != nullptr);

    slot.lobby = slot.slot_memory.alloc<match_slot_lobby>();
    slot.lobby->alloc_start = slot.slot_memory.allocated_count;

    slot.lobby->countdown_active = false;
    slot.lobby->countdown_end_ms = server.uptime_ms + LOBBY_COUNTDOWN_MS;
    slot.lobby->test_mode = server.init_params.test_mode;

	return true;
}

void game_server_slot_fill_lobby_state(game_server& server, match_slot_index slot_index, game_lobby_state& out_state)
{
    match_slot& slot = server.match_slots[slot_index];
    ASSERT(slot.state == MATCH_SLOT_STATE::IN_LOBBY);

	out_state = {};
	out_state.countdown_remaining_ms = (slot.lobby->countdown_active && slot.lobby->countdown_end_ms > server.uptime_ms) ? (ui32)(slot.lobby->countdown_end_ms - server.uptime_ms) : 0;
	out_state.human_count = slot.connection_count;
	out_state.lobby_id = slot_index;
	ia_memcpy(&out_state.match_start_params, slot.match_params, sizeof(game_match_start_params));
	out_state.test_mode = slot.lobby->test_mode;
}

void game_server_slot_send_lobby_state(game_server& server, match_slot_index slot_index)
{
    match_slot& slot = server.match_slots[slot_index];

	ui8 msgBuff[64];
	mem_arena msgMem = mem_arena_create(msgBuff, sizeof(msgBuff));

	game_message_header* msgHeader = build_game_message(GAME_MESSAGE_TYPE::LOBBY_JOINED, msgMem);
	ASSERT(msgHeader);

	game_server_slot_fill_lobby_state(server, slot_index, msgHeader->get_payload_ref<game_message_payload_lobby_joined>().lobby);

	game_server_slot_broadcast(server, slot, *msgHeader);
}

void game_server_fill_lobby_list(game_server& server, game_message_payload_server_lobby_list& out_list)
{
	out_list = {};
	for (match_slot_index slotIndex = 0; slotIndex < server.init_params.match_slot_count && out_list.lobby_count < LOBBY_LIST_MAX_COUNT; slotIndex++)
	{
		if (server.match_slots[slotIndex].state != MATCH_SLOT_STATE::IN_LOBBY) continue;
		game_server_slot_fill_lobby_state(server, slotIndex, out_list.lobbies[out_list.lobby_count++]);
	}
}

void game_server_lobby_handle_join_request(game_server& server, game_server_client& client, const game_message_header& msg)
{
	ASSERT(client.type == game_server_client::TYPE::GAME_CLIENT && client.game_client.authenticated);

	ui8 lobbyID = msg.get_payload_ref<game_message_payload_request_join_lobby>().lobby_id;
	JOIN_LOBBY_RESULT result = JOIN_LOBBY_RESULT::SUCCESS;

	if (client.game_client.match_slot != INVALID_MATCH_SLOT_INDEX)
	{
		result = JOIN_LOBBY_RESULT::ALREADY_IN_LOBBY;
	}
	else if (lobbyID >= server.init_params.match_slot_count || server.match_slots[lobbyID].state != MATCH_SLOT_STATE::IN_LOBBY)
	{
		result = JOIN_LOBBY_RESULT::INVALID_LOBBY;
	}
	else
	{
		ui16 playerIndex;
		if (!game_server_slot_attach_client(server, lobbyID, client.handle, playerIndex)) result = JOIN_LOBBY_RESULT::LOBBY_FULL;
	}

	static_mem_arena<32> msgMem;
	game_message_header* responseMsg = build_game_message(GAME_MESSAGE_TYPE::JOIN_LOBBY_RESPONSE, msgMem);
	ASSERT(responseMsg != nullptr);

	auto& response = responseMsg->get_payload_ref<game_message_payload_join_lobby_response>();
	response.lobby_id = lobbyID;
	response.result = result;
	client.game_client_send_message(*responseMsg);

	if (result == JOIN_LOBBY_RESULT::SUCCESS)
	{
		server.logf("MATCH", LOG_SUCCESS, "Client %d (%s) joined lobby %d as player %d.", client.handle.value, (ia_string_view)client.name, (i32)lobbyID, (i32)client.game_client.player_index);
		game_server_slot_send_lobby_state(server, lobbyID);
	}
}

void match_slot_handle_message_leave_lobby(game_server& server, match_slot_index slot_index, game_server_client* player_client)
{
	// Also accepted once the match started (the client may have left just before): it then leaves the match, as on a disconnection.
	if (game_server_slot_detach_client(server, slot_index, player_client->handle)
		&& server.match_slots[slot_index].state == MATCH_SLOT_STATE::IN_LOBBY)
	{
		game_server_slot_send_lobby_state(server, slot_index);
	}
}

// Lowers or raises the player cap of a lobby. Humans whose player index is above the new cap are moved to a free index below it,
// or kicked (LOBBY_CLIENT_KICKED) if there is none.
static void match_slot_lobby_set_player_count(game_server& server, match_slot_index slot_index, ui16 player_count)
{
	match_slot& slot = server.match_slots[slot_index];
	player_count = ia_clamped(player_count, (ui16)1, LOBBY_MAX_PLAYER_COUNT);

	for (match_player_id playerID = player_count; playerID < slot.match_params->player_count; playerID++)
	{
		game_server_client* playerClient = game_server_get_client_data(server, slot.players[playerID].client);
		if (playerClient == nullptr) continue;

		// Look for a free index under the new cap.
		match_player_id freeID = INVALID_MATCH_PLAYER_ID;
		for (match_player_id otherID = 0; otherID < player_count; otherID++)
		{
			if (game_server_get_client_data(server, slot.players[otherID].client) == nullptr) { freeID = otherID; break; }
		}

		if (freeID != INVALID_MATCH_PLAYER_ID)
		{
			slot.players[freeID] = slot.players[playerID];
			slot.players[playerID] = {};
			playerClient->game_client.player_index = freeID;
			continue;
		}

		static_mem_arena<32> msgMem;
		game_message_header* kickMsg = build_game_message(GAME_MESSAGE_TYPE::LOBBY_CLIENT_KICKED, msgMem);
		ASSERT(kickMsg != nullptr);
		kickMsg->get_payload_ref<game_message_payload_lobby_client_kicked>().reason = LOBBY_KICK_REASON::PLAYER_CAP_LOWERED;
		playerClient->game_client_send_message(*kickMsg);

		game_server_slot_detach_client(server, slot_index, playerClient->handle);
		server.logf("MATCH", LOG_NORMAL, "Kicked client %d from lobby %d: player cap lowered to %d.", playerClient->handle.value, slot_index, player_count);
	}

	// Indices above the cap are all free now.
	slot.match_params->player_count = player_count;
}

void match_slot_handle_message_lobby_admin(game_server& server, match_slot_index slot_index, game_server_client* player_client, const game_message_header& msg)
{
	match_slot& slot = server.match_slots[slot_index];
	if (slot.state != MATCH_SLOT_STATE::IN_LOBBY || !slot.lobby->test_mode)
	{
		server.logf("MATCH", LOG_WARNING, "Client %d sent a lobby admin message outside of a test lobby. Ignored.", player_client->handle.value);
		return;
	}

	switch (msg.message_type)
	{
	case GAME_MESSAGE_TYPE::LOBBY_ADMIN_START_MATCH:
		server.logf("MATCH", LOG_NORMAL, "Client %d started the match of test lobby %d.", player_client->handle.value, slot_index);
		if (!game_server_match_slot_start_play(server, slot_index))
		{
			server.logf("MATCH", LOG_ERROR, "Failed to start match on slot %d !", slot_index);
		}
		return; // No lobby anymore.

	case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_PLAYER_COUNT:
		match_slot_lobby_set_player_count(server, slot_index, msg.get_payload_ref<game_message_payload_lobby_admin_set_player_count>().player_count);
		break;

	case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_MAP_SIZE:
	{
		vec2<ui16> size = msg.get_payload_ref<game_message_payload_lobby_admin_set_map_size>().world_size_regions;
		slot.match_params->world_size_regions.x = ia_clamped(size.x, (ui16)1, LOBBY_MAX_MAP_SIZE_REGIONS);
		slot.match_params->world_size_regions.y = ia_clamped(size.y, (ui16)1, LOBBY_MAX_MAP_SIZE_REGIONS);
		break;
	}

	case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_SEED:
		slot.match_params->random_seed = msg.get_payload_ref<game_message_payload_lobby_admin_set_seed>().random_seed;
		break;

	case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_TICK_RATE:
	{
		ui8 tickRate = msg.get_payload_ref<game_message_payload_lobby_admin_set_tick_rate>().tick_rate;
		if (tickRate < LOBBY_MIN_TICK_RATE || tickRate > LOBBY_MAX_TICK_RATE)
		{
			server.logf("MATCH", LOG_WARNING, "Client %d sent an out of range tick rate (%d). Ignored.", player_client->handle.value, (i32)tickRate);
			return;
		}
		slot.match_params->tick_rate = tickRate;
		break;
	}

	default:
		return;
	}

	game_server_slot_send_lobby_state(server, slot_index);
}

// Ticks a match slot that is currently in its lobby, waiting for players. Starts the match once the countdown ends, with whoever is present.
// Test lobbies never start on their own (only on LOBBY_ADMIN_START_MATCH).
void game_server_tick_match_slot_lobby(game_server& server, ui8 slot_index)
{
    match_slot& slot = server.match_slots[slot_index];

    if (slot.lobby->test_mode)
    {
        // Every second, broadcast current lobby state.
        if (server.uptime_ms > slot.lobby->lobby_state_refresh_time)
        {
            slot.lobby->lobby_state_refresh_time = server.uptime_ms + 1000;
            game_server_slot_send_lobby_state(server, slot_index);
        }
        return;
    }

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
	if (!game_server_match_slot_start_play(server, slot_index))
    {
        server.logf("MATCH", LOG_ERROR, "Failed to start match on slot %d !", slot_index);
    }
}
