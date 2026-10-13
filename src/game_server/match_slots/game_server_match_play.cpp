// Implementation file for match slots in ONGOING state / slots in play.

#include "core.h"
#include "core/math/random.h"
#include "core/memory.h"
#include "game_common/game_messages.h"
#include "game_common/match/commands.h"
#include "game_common/match/world.h"

#include "match_slots.h"
#include "../game_server.h"

// Returns whether the location is a valid settlement location for the player, and at least SETTLEMENT_MIN_DISTANCE tiles from the current pick of every other player.
static bool match_is_valid_pick(game_server& server, ui8 slot_index, match_player_id player, world_location loc)
{
    match_slot& slot = server.match_slots[slot_index];
    if (!world_is_valid_settlement_location(*slot.match.match_ptr->world, loc, player)) return false;

    for (match_player_id otherID = 0; otherID < slot.match_params->player_count; otherID++)
    {
        world_location otherLoc = slot.match.start_locations[otherID];
        if (otherID == player || otherLoc == INVALID_WORLD_LOCATION) continue; // Not picked yet.

        i32 dx = (i32)otherLoc.x - (i32)loc.x;
        i32 dy = (i32)otherLoc.y - (i32)loc.y;
        if (dx * dx + dy * dy < (i32)(SETTLEMENT_MIN_DISTANCE * SETTLEMENT_MIN_DISTANCE)) return false;
    }
    return true;
}

// Picks the start location of a player and broadcasts it. Invalid locations are discarded silently, unless skip_validity_check is set.
void match_pick_start_location(game_server& server, ui8 slot_index, match_player_id player, world_location picked_loc, bool skip_validity_check = false)
{
    match_slot& slot = server.match_slots[slot_index];
    ASSERT(slot.match_params->player_count > player);
    ASSERT(slot.match.start_locations != nullptr);

    if (!skip_validity_check && !match_is_valid_pick(server, slot_index, player, picked_loc)) return;

    slot.match.start_locations[player] = picked_loc;

    // Broadcast choice to all players.
    static_mem_arena<32> messageMem;
    game_message_header* serverPlacementPickMsg = build_game_message(GAME_MESSAGE_TYPE::SERVER_PLACEMENT_PICK, messageMem, 0);
    ASSERT(serverPlacementPickMsg != nullptr);

    game_message_payload_placement_pick& pickPayload = serverPlacementPickMsg->get_payload_ref<game_message_payload_placement_pick>();
    pickPayload.player_id = player;
    pickPayload.picked_loc = picked_loc;

    game_server_slot_broadcast(server, slot, *serverPlacementPickMsg);
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

    // Free memory taken by lobby structure.
    slot.slot_memory.allocated_count = slot.lobby->alloc_start;

	// Create match.

	slot.match = {}; // Reset memory associated with the match state.

	slot.match.match_ptr = slot.slot_memory.alloc<game_match>();

	game_match& match = *slot.match.match_ptr;
	if (!match_start(slot.slot_memory, server.uptime_ms, *slot.match_params, match))
	{
		server.logf("MATCH", LOG_ERROR, "Failed to start match in slot %d.", slot_index);
		slot.state = MATCH_SLOT_STATE::AWAITING_CLEANUP;
		return false;
	}

	slot.state = MATCH_SLOT_STATE::MATCH_ONGOING;

	// Send MATCH STARTED message to all connected clients.
    i16 humanPlayerCount = 0;
    {
        ui8 msg_scratch_buff[2048];
        mem_arena msg_scratch_mem = mem_arena_create(msg_scratch_buff, sizeof(msg_scratch_buff));

        // Allocate message, adding extra data size of the match parameters as extra bytes.

        game_message_header* joinMsgHeader = build_game_message(GAME_MESSAGE_TYPE::MATCH_STARTED, msg_scratch_mem, 0);
        ASSERT(joinMsgHeader);

        auto& joinMsgPayload = joinMsgHeader->get_payload_ref<game_message_payload_match_started>();
        joinMsgPayload = {};
        joinMsgPayload.placement_countdown_ms = PLACEMENT_COUNTDOWN_MS;

        // Copy start params.
        ia_memcpy(&joinMsgPayload.match_start_params, slot.match_params, sizeof(game_match_start_params));

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

    // Begin placement phase
    slot.match.in_placement = true;
    slot.match.start_time = server.uptime_ms + PLACEMENT_COUNTDOWN_MS;

    slot.match.start_locations = slot.slot_memory.alloc<world_location>(slot.match_params->player_count);
    ASSERT(slot.match.start_locations != nullptr);
    for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
    {
        slot.match.start_locations[playerID] = INVALID_WORLD_LOCATION; // Marks "not picked yet".
    }

    // Pick a random location for all players to begin with, retrying a few times to find a valid one (else the last one is accepted anyway).
    rand_generator_32 randGen = { match.start_params->random_seed };
    for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
    {
        world_location randLoc;
        for (ui8 tryIndex = 0; tryIndex < 32; tryIndex++)
        {
            randLoc = randGen.next_vec2({10, 10}, match.start_params->world_size_regions * world_terrain::REGION_SIZE - world_location {10, 10});
            if (match_is_valid_pick(server, slot_index, playerID, randLoc)) break;
        }
        match_pick_start_location(server, slot_index, playerID, randLoc, true);
    }

    server.logf("MATCH", LOG_SUCCESS, "Match started on slot %d.", slot_index);

	return true;
}

void match_slot_handle_message_client_placement_pick(game_server& server, match_slot_index slot_index, game_server_client* player_client, const game_message_header& msg)
{
    match_slot& slot = server.match_slots[slot_index];

    if (slot.state != MATCH_SLOT_STATE::MATCH_ONGOING || !slot.match.in_placement) return;

    world_location pickedLoc;

    const auto& payload = msg.get_payload_ref<game_message_payload_placement_pick>();
    pickedLoc = payload.picked_loc;

    match_pick_start_location(server, slot_index, player_client->game_client.player_index, pickedLoc);
}

void match_slot_handle_message_client_tick(game_server& server, match_slot_index slot_index, game_server_client* player_client, const game_message_header& msg)
{
    match_slot& slot = server.match_slots[slot_index];

    if (slot.match.in_placement) return; // Ignore all client tick messages until placement phase is over.

    const auto& payload = msg.get_payload_ref<game_message_payload_client_tick>();
    if (slot.match.next_tick_commands_builder._tick_commands_start != nullptr 
            && payload.emit_tick <= slot.match.match_ptr->tick // Unsigned: a future emit tick would wrap below.
            && slot.match.match_ptr->tick - payload.emit_tick < slot.match_params->tick_rate) // Don't take command into account if it was emitted on too old a tick.
    {
        slot.match.next_tick_commands_builder.push_validated_sequence(player_client->game_client.player_index, *slot.match.match_ptr, payload.commands);
    }
}

// Returns the player controlling at least MATCH_END_POPULATION_PERCENT of the world's total settlement population, or INVALID_MATCH_PLAYER_ID if none does.
static match_player_id match_find_dominant_player(const match_world_state& world)
{
	const world_entity_settlements& settlements = world.entities.settlements;

	// Weighted majority vote (Boyer-Moore) finds the only player who could hold more than half the population, without per-player storage.
	ui64 totalPopulation = 0;
	match_player_id candidate = INVALID_MATCH_PLAYER_ID;
	ui64 votes = 0;
	for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid()) continue;

		ui64 population = settlements.populations[settlementIndex];
		totalPopulation += population;

		if (settlements.owners[settlementIndex] == candidate) votes += population;
		else if (votes >= population) votes -= population;
		else
		{
			candidate = settlements.owners[settlementIndex];
			votes = population - votes;
		}
	}

	if (totalPopulation == 0 || candidate == INVALID_MATCH_PLAYER_ID) return INVALID_MATCH_PLAYER_ID;

	ui64 candidatePopulation = 0;
	for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (settlements.guids[settlementIndex].is_valid() && settlements.owners[settlementIndex] == candidate) candidatePopulation += settlements.populations[settlementIndex];
	}

	return (candidatePopulation * 100 >= totalPopulation * MATCH_END_POPULATION_PERCENT) ? candidate : INVALID_MATCH_PLAYER_ID;
}

// Begins match slot, triggering it to spawn every player at their chosen start location.
static void match_begin(game_server& server, ui8 slot_index)
{
    match_slot& slot = server.match_slots[slot_index];

    slot.match.next_tick_commands_builder.push_new_sequence(INVALID_MATCH_PLAYER_ID);

    // Spawn players at chosen start position.
    for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
    {
        world_location& startLoc = slot.match.start_locations[playerID];

        auto& spawnPlayer = *slot.match.next_tick_commands_builder.push_command<command_data_server_spawn_player>(MATCH_COMMAND_TYPE::SERVER_SPAWN_PLAYER);
        spawnPlayer.spawned_player = playerID;
        spawnPlayer.spawn_location = startLoc;
    }

    slot.match.last_tick_time = server.uptime_ms;
    slot.match.in_placement = false;
}

// Ticks a match slot with an ongoing match: gathers input, simulates & relays as many ticks as have elapsed since last time.
void game_server_tick_match_slot_ongoing(game_server& server, ui8 slot_index)
{
	match_slot& slot = server.match_slots[slot_index];

	// End match early if all players disconnect.
	if (slot.connection_count == 0)
	{
		server.logf("MATCH", LOG_WARNING, "All players have left match on slot %d. Ending match early.", slot_index);
		game_server_end_match_slot(server, slot_index, INVALID_MATCH_PLAYER_ID);
		return;
	}

    if (slot.match.in_placement)
    {
        if (slot.match.start_time <= server.uptime_ms)
        {
            match_begin(server, slot_index);
        }
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

		// Placeholder end condition, checked every few ticks.
		// Never with a single player: they would trivially hold 100% from the start.
		if (slot.match_params->player_count > 1 && slot.match.match_ptr->tick % MATCH_END_CHECK_INTERVAL_TICKS == 0)
		{
			match_player_id winner = match_find_dominant_player(*slot.match.match_ptr->world);
			if (winner != INVALID_MATCH_PLAYER_ID)
			{
				server.logf("MATCH", LOG_NORMAL, "Player %hu controls %llupct or more of the world population. Ending match on slot %d.", winner, MATCH_END_POPULATION_PERCENT, slot_index);
				game_server_end_match_slot(server, slot_index, winner);
				return;
			}
		}

        // Reset input commands memory & builder.
        slot.match.next_tick_commands_memory.clear();
        slot.match.next_tick_commands_builder = { &slot.match.next_tick_commands_memory };
        slot.match.next_tick_commands_builder.init();

		slot.match.last_tick_time = nextTickTimeMs;
		nextTickTimeMs += msPerTick;
	}
}

bool game_server_end_match_slot(game_server& server, ui8 slot_index, match_player_id winner_id)
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

	// Notify attached clients.
	ui8 msgBuff[64];
	mem_arena msgMem = mem_arena_create(msgBuff, sizeof(msgBuff));

	game_message_header* msgHeader = build_game_message(GAME_MESSAGE_TYPE::MATCH_ENDED, msgMem);
	ASSERT(msgHeader);
	msgHeader->get_payload_ref<game_message_payload_match_ended>().winner_id = winner_id;

	game_server_slot_broadcast(server, slot, *msgHeader);

	return true;
}
