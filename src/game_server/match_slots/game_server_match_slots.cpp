// Main Implementation of Game Server match slots system.
// Match slots are the persistent "places" in server memory going through a cycle of free -> lobby -> match -> end, providing continued
// match hosting service.

#ifndef GAME_SERVER_MATCH_SLOTS_INCLUDED
#define GAME_SERVER_MATCH_SLOTS_INCLUDED

// Game Message protocol + match & match commands system.
#include "game_common/game_messages.h"
#include "game_common/match/match.h"
#include "game_common/match/commands.h"

#include "game_common/match/world.h"
#include "match_slots.h"
#include "../game_server.h"

static constexpr time_ms LOBBY_COUNTDOWN_MS = 60000; // Time between the first client joining a lobby and its match starting.
static constexpr ui32 MATCH_END_CHECK_INTERVAL_TICKS = 30; // Placeholder end condition is checked every this many ticks.
static constexpr ui64 MATCH_END_POPULATION_PERCENT = 80; // Placeholder end condition: one player controls this much of the world population.

// Sends a message to every live game client attached to the slot.
static void game_server_slot_broadcast(game_server& server, match_slot& slot, const game_message_header& msg)
{
	for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
	{
		game_server_client* playerClient = game_server_get_client_data(server, slot.players[playerID].client);
		if (playerClient == nullptr || playerClient->type != game_server_client::TYPE::GAME_CLIENT) continue;

		playerClient->game_client_send_message(msg);
	}
}

// Sends the lobby's state (countdown, player count, slot index, match params) to every client in it.
static void game_server_slot_send_lobby_state(game_server& server, match_slot& slot, match_slot_index slot_index)
{
	ui8 msgBuff[64];
	mem_arena msgMem = mem_arena_create(msgBuff, sizeof(msgBuff));

	game_message_header* msgHeader = build_game_message(GAME_MESSAGE_TYPE::LOBBY_JOINED, msgMem);
	ASSERT(msgHeader);

	auto& payload = msgHeader->get_payload_ref<game_message_payload_lobby_joined>();
	payload.countdown_remaining_ms = (slot.countdown_active && slot.countdown_end_ms > server.uptime_ms) ? (ui32)(slot.countdown_end_ms - server.uptime_ms) : 0;
	payload.lobby_player_count = slot.connection_count;
	payload.match_slot_index = slot_index;
	ia_memcpy(&payload.match_start_params, slot.match_params, sizeof(game_match_start_params));

	game_server_slot_broadcast(server, slot, *msgHeader);
}

// Returns the player controlling at least MATCH_END_POPULATION_PERCENT of the world's total settlement population, or INVALID_MATCH_PLAYER_ID if none does.
static match_player_id game_server_find_dominant_player(const match_world_state& world)
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

void match_slot_handle_message_client_tick(match_slot& slot, game_server_client* player_client, const game_message_header& msg)
{
    const auto& payload = msg.get_payload_ref<game_message_payload_client_tick>();
    if (slot.match.next_tick_commands_builder._tick_commands_start != nullptr 
            && payload.emit_tick <= slot.match.match_ptr->tick // Unsigned: a future emit tick would wrap below.
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

    // Spawn players at random by pushing spawn commands to be processed for the first tick.
    // @TODO(Marc): Determine start locations in lobby.

    slot.match.next_tick_commands_builder.push_new_sequence(INVALID_MATCH_PLAYER_ID);
    for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
    {
        command_data_server_spawn_player* spawnPlayer = slot.match.next_tick_commands_builder
            .push_command<command_data_server_spawn_player>(MATCH_COMMAND_TYPE::SERVER_SPAWN_PLAYER);

        spawnPlayer->spawned_player = playerID;
        spawnPlayer->spawn_location = match.world->rand_gen.next_vec2(world_dimensions {50, 50}, match.world->terrain.size_tiles - world_dimensions{50, 50});
    }

    server.logf("MATCH", LOG_SUCCESS, "Match started on slot %d.", slot_index);

	return true;
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

bool game_server_reset_match_slot(game_server& server, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	if (slot.state != MATCH_SLOT_STATE::MATCH_ENDED && slot.state != MATCH_SLOT_STATE::AWAITING_CLEANUP)
	{
		server.logf("MATCH", LOG_ERROR, "Attempted to reset match slot %d which wasn't ended or awaiting cleanup.", slot_index);
		return false;
	}

	server.logf("MATCH", LOG_NORMAL, "Resetting server match slot index %d.", slot_index);

    // Detach all attached clients if they're still here, so they can be attached to a lobby again.
    // Checked on the live client rather than bIsClientPlayer, which is only set once a match starts.
    for (match_player_id playerID = 0; slot.match_params != nullptr && slot.players != nullptr && playerID < slot.match_params->player_count; playerID++)
    {
        if (game_server_get_client_data(server, slot.players[playerID].client) != nullptr)
        {
            game_server_slot_detach_client(server, slot_index, slot.players[playerID].client);
        }
    }

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

	// Only lobbies accept clients: the caller is free to try every slot.
	if (slot.state != MATCH_SLOT_STATE::IN_LOBBY) return false;

	game_server_client* clientPtr = game_server_get_client_data(server, client_handle);
	ASSERT(clientPtr != nullptr);	
	
	game_server_client& client = *clientPtr;
    server.logf("MATCH", LOG_NORMAL, "Attaching client %d, PLAYER INDEX = %hu", client_handle.value, client.game_client.player_index);

	ASSERT(client.type == game_server_client::TYPE::GAME_CLIENT);
    ASSERT(client.game_client.match_slot == INVALID_MATCH_SLOT_INDEX);
    ASSERT(client.game_client.player_index == INVALID_MATCH_PLAYER_ID);

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

    slot.connection_count++;

	// The first client to join starts the countdown.
	if (!slot.countdown_active)
	{
		slot.countdown_active = true;
		slot.countdown_end_ms = server.uptime_ms + LOBBY_COUNTDOWN_MS;
	}

	// All player indices filled: no reason to wait for the countdown.
	if (slot.connection_count >= slot.match_params->player_count)
	{
		server.logf("MATCH", LOG_NORMAL, "Lobby on slot %d is full. Starting match immediately.", slot_index);
		if (!game_server_start_match_slot(server, slot_index))
		{
			server.logf("MATCH", LOG_ERROR, "Failed to start match on slot %d !", slot_index);
		}
		return true; // The client is attached either way.
	}

	game_server_slot_send_lobby_state(server, slot, slot_index);

	return true;
}

bool game_server_slot_detach_client(game_server& server, match_slot_index slot_index, game_server_client::client_handle client_handle)
{
    if (slot_index >= server.init_params.match_slot_count) return false; // Client isn't attached to any slot.

    match_slot& slot = server.match_slots[slot_index];
    if (slot.match_params == nullptr || slot.players == nullptr) return false;

    match_player_id freedSlot = INVALID_MATCH_PLAYER_ID;

    // If client is active on the server and has valid data, we can just check its slot index and lookup its player index.
    // For robustness, any failure skip to the full check in case data has become incoherent.
    game_server_client* clientPtr = game_server_get_client_data(server, client_handle);
    if (clientPtr != nullptr)
    {
        if (clientPtr->type == game_server_client::TYPE::GAME_CLIENT 
        && clientPtr->game_client.match_slot == slot_index
        && clientPtr->game_client.player_index < slot.match_params->player_count
        && slot.players[clientPtr->game_client.player_index].client.value == client_handle.value)
        {
            freedSlot = clientPtr->game_client.player_index;

            slot.players[clientPtr->game_client.player_index].client = {};
            slot.players[clientPtr->game_client.player_index].bIsClientPlayer = false;

            clientPtr->game_client.match_slot = INVALID_MATCH_SLOT_INDEX;
            clientPtr->game_client.player_index = INVALID_MATCH_PLAYER_ID;

            goto SUCCESS;
        }
    }

    // Game client is no longer valid or has incoherent data. Perform a full check and free up whatever player index the handle takes up, if any.
    for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
    {
        if (slot.players[playerID].bIsClientPlayer && slot.players[playerID].client.value == client_handle.value)
        {
            freedSlot = playerID;

            slot.players[playerID].client = {};
            slot.players[playerID].bIsClientPlayer = false;

            goto SUCCESS;   
        }
    }

NO_PLAYER:
    return false;

SUCCESS:
    ASSERT(slot.connection_count > 0);
    slot.connection_count--;

    // In a lobby the seat is freed. An empty lobby resets its countdown, otherwise the others see the new player count.
    // In a match the player index stays reserved with no input (no AI takeover).
    if (slot.state == MATCH_SLOT_STATE::IN_LOBBY)
    {
        if (slot.connection_count == 0) slot.countdown_active = false;
        else game_server_slot_send_lobby_state(server, slot, slot_index);
    }

    server.logf("MATCH", LOG_NORMAL, "Detached client %d from match slot %d. Freed player index = %hu.", client_handle.value, slot_index, freedSlot);
    return true;
}

// Ticks a match slot that is currently in its lobby, waiting for players. Starts the match once the countdown ends, with whoever is present.
static void game_server_tick_match_slot_lobby(game_server& server, ui8 slot_index)
{
    match_slot& slot = server.match_slots[slot_index];

	if (!slot.countdown_active || server.uptime_ms < slot.countdown_end_ms) return; // Countdown starts when the first player joins.

	// On failure the slot goes to AWAITING_CLEANUP and gets reset.
	if (!game_server_start_match_slot(server, slot_index))
    {
        server.logf("MATCH", LOG_ERROR, "Failed to start match on slot %d !", slot_index);
    }
}

// Ticks a match slot with an ongoing match: gathers input, simulates & relays as many ticks as have elapsed since last time.
static void game_server_tick_match_slot_ongoing(game_server& server, ui8 slot_index)
{
	match_slot& slot = server.match_slots[slot_index];

	// End match early if all players disconnect.
	if (slot.connection_count == 0)
	{
		server.logf("MATCH", LOG_WARNING, "All players have left match on slot %d. Ending match early.", slot_index);
		game_server_end_match_slot(server, slot_index, INVALID_MATCH_PLAYER_ID);
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
			match_player_id winner = game_server_find_dominant_player(*slot.match.match_ptr->world);
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

void game_server_tick_match_slots(game_server& server)
{
	// Manage match slots.
	for (ui8 slotIndex = 0; slotIndex < server.init_params.match_slot_count; slotIndex++)
	{
		match_slot& slot = server.match_slots[slotIndex];

		switch (slot.state)
		{
        case MATCH_SLOT_STATE::WAITING:
            // Wait for server to open lobby.
            break;
		case MATCH_SLOT_STATE::IN_LOBBY:
			game_server_tick_match_slot_lobby(server, slotIndex);
			break;
		case MATCH_SLOT_STATE::MATCH_ONGOING:
			game_server_tick_match_slot_ongoing(server, slotIndex);
			break;
        case MATCH_SLOT_STATE::MATCH_ENDED:
            // ... TODO: Wait until match results have been transmitted to all player & master server.

            // FALLTHROUGH
        case MATCH_SLOT_STATE::AWAITING_CLEANUP:
            // Reset slot.
            game_server_reset_match_slot(server, slotIndex);
            break;
		default:
			// Do nothing.
			break;
		}
	}
}

// END MATCH SLOT SYSTEM IMPLEMENTATION



#endif // GAME_SERVER_MATCH_SLOTS_INCLUDED
