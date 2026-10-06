// Main Implementation of Game Server match slots system.
// Match slots are the persistent "places" in server memory going through a cycle of free -> lobby -> match -> end, providing continued
// match hosting service.

#ifndef GAME_SERVER_MATCH_SLOTS_INCLUDED
#define GAME_SERVER_MATCH_SLOTS_INCLUDED

// Game Message protocol + match & match commands system.
#include "game_common/game_messages.h"
#include "game_common/match/match.h"
#include "game_common/match/commands.h"

#include "match_slots.h"
#include "../game_server.h"

void match_slot_handle_message_client_tick(match_slot& slot, game_server_client* player_client, const game_message_header& msg)
{
    const auto& payload = msg.get_payload_ref<game_message_payload_client_tick>();
    if (slot.match.next_tick_commands_builder._tick_commands_start != nullptr 
            && slot.match.match_ptr->tick - payload.emit_tick < slot.match_params->tick_rate) // Don't take command into account if it was emitted on too old a tick.
    {
        slot.match.next_tick_commands_builder.push_validated_sequence(player_client->game_client.player_index, *slot.match.match_ptr, payload.commands);
    }
}

// Handles a game message from the client. Returns whether the message was handled.
bool game_server_match_slot_handle_message(game_server &server, game_server_client::client_handle client_handle, const game_message_header &msg)
{
    game_server_client* playerClient = game_server_get_client_data(server, client_handle);
    if (playerClient == nullptr || playerClient->type != game_server_client::TYPE::GAME_CLIENT) return false;
    if (playerClient->game_client.match_slot >= server.init_params.match_slot_count) return false;

    match_slot_index slotIndex = playerClient->game_client.match_slot;
    match_slot& slot = server.match_slots[slotIndex];

    switch(msg.message_type)
    {
    case GAME_MESSAGE_TYPE::CLIENT_TICK:
        match_slot_handle_message_client_tick(slot, playerClient, msg);
        return true;
    default:
        return false;
    }
}

// BEGIN SLOT LIFECYCLE FUNCTIONS

bool game_server_init_match_slot(game_server& server, mem_arena& slot_mem, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	if (slot.state != MATCH_SLOT_STATE::UNINITIALIZED)
	{
		server.logf("MATCH", LOG_ERROR, "Match slot index %d is already initialized (current state = %d).", slot_index, slot.state);
		return false;
	}

	server.logf("MATCH", LOG_NORMAL, "Initializing server match slot index %d.", slot_index);

	slot = {};
	slot.state = MATCH_SLOT_STATE::WAITING;
	slot.slot_memory = slot_mem;

	return true;
}

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

	// TO IMPLEMENT: Lobby system, tied to the client connection system / player system (abstraction might be convenient for AI / bot players ?)
	slot.players = slot.slot_memory.alloc<match_player>(slot.match_params->player_count);
	ASSERT(slot.players != nullptr);

	return true;
}

bool game_server_start_match_slot(game_server& server, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	
	if (slot.state != MATCH_SLOT_STATE::IN_LOBBY)
	{
		server.logf("MATCH", LOG_ERROR, "Attempted to start match in slot %d which wasn't in lobby.", slot_index);
		return false;
	}

	server.logf("MATCH", LOG_NORMAL, "Starting match in match slot %d.", slot_index);

	// Create match.

	slot.match = {}; // Reset memory associated with the match state.

	slot.match.match_ptr = slot.slot_memory.alloc<game_match>();
	slot.match.last_tick_time = server.uptime_ms;

	game_match& match = *slot.match.match_ptr;
	if (!match_start(slot.slot_memory, server.uptime_ms, *slot.match_params, match))
	{
		server.logf("MATCH", LOG_ERROR, "Failed to start match in slot %d.", slot_index);
		slot.state = MATCH_SLOT_STATE::AWAITING_CLEANUP;
		return false;
	}

	slot.state = MATCH_SLOT_STATE::MATCH_ONGOING;

	// Send MATCH JOINED message to all connected clients.
    i16 humanPlayerCount = 0;
    {
        ui8 msg_scratch_buff[2048];
        mem_arena msg_scratch_mem = mem_arena_create(msg_scratch_buff, sizeof(msg_scratch_buff));

        // Allocate message, adding extra data size of the match parameters as extra bytes.

        game_message_header* joinMsgHeader = build_game_message(GAME_MESSAGE_TYPE::MATCH_JOINED, msg_scratch_mem, slot.match_params->extra_data_size);
        ASSERT(joinMsgHeader);

        auto& joinMsgPayload = joinMsgHeader->get_payload_ref<game_message_payload_match_joined>();
        joinMsgPayload = {};

        // Copy start params.
        ia_memcpy(&joinMsgPayload.match_start_params, slot.match_params, sizeof(game_match_start_params) + slot.match_params->extra_data_size);

        for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
        {
            match_player& playerSlot = slot.players[playerID];
            game_server_client* playerClient = game_server_get_client_data(server, playerSlot.client);
            if (playerClient == nullptr || playerClient->type != game_server_client::TYPE::GAME_CLIENT)
            {
                continue;
            }

            playerSlot.bIsClientPlayer = true;

            joinMsgPayload.controlled_player_id = playerID;
            joinMsgPayload.join_tick = 0;

            // Send message !
            playerClient->game_client_send_message(*joinMsgHeader);

            humanPlayerCount++;
        }
    }

    // Allocate room for AI players in match memory.
    ui16 AIPlayerCount = ia_min(256, slot.match_params->player_count - humanPlayerCount);
    if (AIPlayerCount > 0)
    {
        mem_arena AIPlayersMem = mem_arena_create_sub(slot.slot_memory, AIPlayerCount * sizeof(AI_player_state));
        ASSERT(AIPlayersMem.mem_start != nullptr);
        AIPlayersMem.clear();

        // Alloc AI players over all un-occupied player indices.
        for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
        {
            match_player& playerSlot = slot.players[playerID];
            game_server_client* playerClient = game_server_get_client_data(server, playerSlot.client);
            if (playerClient == nullptr || playerClient->type != game_server_client::TYPE::GAME_CLIENT)
            {
                playerSlot.bIsClientPlayer = false;

                // Alloc AI player for this player index.
                if (AI_player_state* newAI = AIPlayersMem.alloc<AI_player_state>())
                {
                    newAI->controlled_player = playerID;
                    slot.match.ai_player_count++;
                }
                continue;
            }
        }

        // Link AI players buffer.
        slot.match.ai_players = (AI_player_state*)AIPlayersMem.mem_start;
    }

    // Allocate input commands memory according to total player count.
    slot.match.next_tick_commands_memory = mem_arena_create_sub(slot.slot_memory, 64 * slot.match_params->player_count);
    slot.match.next_tick_commands_builder = { &slot.match.next_tick_commands_memory };
    slot.match.next_tick_commands_builder.init();

    server.logf("MATCH", LOG_SUCCESS, "Match started on slot %d.", slot_index);

	return true;
}

bool game_server_end_match_slot(game_server& server, ui8 slot_index)
{	
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	
	if (slot.state != MATCH_SLOT_STATE::MATCH_ONGOING)
	{
		server.logf("MATCH", LOG_ERROR, "Attempted to end match in slot %d which wasn't ongoing.", slot_index);
		return false;
	}

	server.logf("MATCH", LOG_NORMAL, "Ending match in match slot %d.", slot_index);

	slot.state = MATCH_SLOT_STATE::MATCH_ENDED;

	// TO IMPLEMENT.

	return true;
}

bool game_server_reset_match_slot(game_server& server, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	if (slot.state != MATCH_SLOT_STATE::MATCH_ENDED)
	{
		server.logf("MATCH", LOG_ERROR, "Attempted to reset match slot %d which wasn't and ended match.", slot_index);
		return false;
	}

	server.logf("MATCH", LOG_NORMAL, "Resetting server match slot index %d.", slot_index);

	// Zero out the slot and set it back to waiting. Conserve only its memory.

	mem_arena slot_mem = slot.slot_memory;

	slot = {};
	slot.state = MATCH_SLOT_STATE::WAITING;
	
	// Reset memory.
	slot.slot_memory = slot_mem;
    slot.slot_memory.clear();

	// From there the slot can be put into lobby mode to start accepting players, or directly have its parameters set and match started.

	return true;
}

bool game_server_slot_attach_client(game_server& server, ui8 slot_index, game_server_client::client_handle client_handle, ui16& out_player_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);
	match_slot& slot = server.match_slots[slot_index];

	ASSERT(slot.state == MATCH_SLOT_STATE::IN_LOBBY
		|| slot.state == MATCH_SLOT_STATE::MATCH_ONGOING);

	game_server_client* clientPtr = game_server_get_client_data(server, client_handle);
	ASSERT(clientPtr != nullptr);	
	
	game_server_client& client = *clientPtr;
	ASSERT(client.type == game_server_client::TYPE::GAME_CLIENT);


	if (slot.match_params == nullptr)
	{
		server.logf("MATCH", LOG_ERROR, "Attempted to attach a client to match slot %d, which has no match parameters set.", slot_index);
		return false;
	}

	bool attachSuccessful = false;
	for (ui16 playerIndex = 0; playerIndex < slot.match_params->player_count; playerIndex++)
	{
		match_player& player = slot.players[playerIndex];
		if (game_server_get_client_data(server, player.client) != nullptr) continue;	// NOTE(Marc): Skip over any slot that aren't CURRENTLY assigned to a live client.
																						// Later this will change to being any slot that was NEVER assigned to a client.
		player.client = client_handle;
		out_player_index = playerIndex;
        client.game_client.player_index = playerIndex;
        client.game_client.match_slot = slot_index;
		attachSuccessful = true;
		break;
	}

	if (!attachSuccessful)
	{
		server.logf("MATCH", LOG_WARNING, "Match slot %d has no free player index to attach a client to.", slot_index);
		return false;
	}
	return true;
}

// Ticks a match slot that is currently in its lobby, waiting for players. Starts the match once enough are connected.
static void game_server_tick_match_slot_lobby(game_server& server, ui8 slotIndex, ui8 connectionCount)
{
	if (connectionCount < 2) return; // Wait until a player connects.

	game_server_start_match_slot(server, slotIndex);
}

// Ticks a match slot with an ongoing match: gathers input, simulates & relays as many ticks as have elapsed since last time.
static void game_server_tick_match_slot_ongoing(game_server& server, ui8 slotIndex, ui8 connectionCount)
{
	match_slot& slot = server.match_slots[slotIndex];

	// End match early if all players disconnect.
	if (connectionCount == 0)
	{
		server.logf("MATCH", LOG_WARNING, "All players have left match on slot %d. Ending match early.", slotIndex);
		game_server_end_match_slot(server, slotIndex);

		// TEMP: Until the full match lifecycle + clients delayed joining are here, just shut the server down along with the match.
		server.platform->shutdown(0);
		return;
	}

	ui64 msPerTick = (1000 / slot.match_params->tick_rate);

	// TODO: Avoid starvation by budgeting ticking time on each slot and ticking each once evenly instead of catching each one up then the next.
	time_ms nextTickTimeMs = slot.match.last_tick_time + msPerTick;
	while (nextTickTimeMs < server.uptime_ms)
	{
		// Remember how much memory was allocated at this point, so the throwaway per-tick command data below can be reclaimed once it's been sent.
		ui32 slotMemoryAllocated = slot.slot_memory.allocated_count;

		// Gather AI input.
		for (ui16 aiPlayerIndex = 0; aiPlayerIndex < slot.match.ai_player_count; aiPlayerIndex++)
		{
			AI_player_state& aiState = slot.match.ai_players[aiPlayerIndex];
			AI_output_commands(*slot.match.match_ptr, aiState, slot.match.next_tick_commands_builder);
		}

		match_tick_commands& tickCommands = *slot.match.next_tick_commands_builder._tick_commands_start;

		// Send tick update to all players.

		ui8 update_buff[2048];
		mem_arena update_mem = mem_arena_create(update_buff, sizeof(update_buff));

		game_message_header* tickMsgHeader = build_game_message(GAME_MESSAGE_TYPE::SERVER_TICK, update_mem, tickCommands.total_size);
		ASSERT(tickMsgHeader != nullptr); // TODO(Marc): extendable memory ? We may have a LOT of input to deal with at times.

		auto& serverTickPayload = tickMsgHeader->get_payload_ref<game_message_payload_server_tick>();
		serverTickPayload.apply_tick = slot.match.match_ptr->tick - 1;

		// Copy the whole tick commands structure (header + trailing sequence buffer) in one go.
		ia_memcpy(&serverTickPayload.commands, &tickCommands, sizeof(match_tick_commands) + tickCommands.total_size);

		for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
		{
			game_server_client* playerClient = game_server_get_client_data(server, slot.players[playerID].client);
			if (playerClient == nullptr || playerClient->type != game_server_client::TYPE::GAME_CLIENT) continue;

			playerClient->game_client_send_message(*tickMsgHeader);
		}

		// Perform server-side match tick.
		match_tick(*slot.match.match_ptr, tickCommands);

        // Reset input commands memory & builder.
        slot.match.next_tick_commands_memory.clear();
        slot.match.next_tick_commands_builder = { &slot.match.next_tick_commands_memory };
        slot.match.next_tick_commands_builder.init();

		slot.match.last_tick_time = nextTickTimeMs;
		nextTickTimeMs += msPerTick;
	}
}

void game_server_tick_match_slots(game_server& server)
{
	// Manage match slots.
	for (ui8 slotIndex = 0; slotIndex < server.init_params.match_slot_count; slotIndex++)
	{
		match_slot& slot = server.match_slots[slotIndex];

		// TEST: Count how many active connections there are. Once there are 2, start the match.
		ui8 connectionCount = 0;

		if (slot.state == MATCH_SLOT_STATE::IN_LOBBY || slot.state == MATCH_SLOT_STATE::MATCH_ONGOING)
		{
			for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
			{
				const game_server_client* playerClient = game_server_get_client_data(server, slot.players[playerID].client);
				if (playerClient == nullptr || playerClient->type != game_server_client::TYPE::GAME_CLIENT) continue;
				connectionCount++;
			}
		}

		switch (slot.state)
		{
		case MATCH_SLOT_STATE::IN_LOBBY:
			game_server_tick_match_slot_lobby(server, slotIndex, connectionCount);
			break;
		case MATCH_SLOT_STATE::MATCH_ONGOING:
			game_server_tick_match_slot_ongoing(server, slotIndex, connectionCount);
			break;
		default:
			// Do nothing.
			break;
		}
	}
}

// END MATCH SLOT SYSTEM IMPLEMENTATION



#endif // GAME_SERVER_MATCH_SLOTS_INCLUDED
