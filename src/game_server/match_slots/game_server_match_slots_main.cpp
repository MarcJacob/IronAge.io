// Main Implementation of Game Server match slots system.
// Match slots are the persistent "places" in server memory going through a cycle of free -> lobby -> match -> end, providing continued
// match hosting service.

// Game Message protocol + match & match commands system.
#include "game_common/game_messages.h"
#include "game_common/match/match.h"

#include "game_common/match/world/world.h"
#include "match_slots.h"
#include "../game_server.h"

// Unity include slot state implementation files.
#include "game_server_match_in_lobby.cpp"
#include "game_server_match_in_play.cpp"

// Sends a message to every live game client attached to the slot.
void game_server_slot_broadcast(game_server& server, match_slot& slot, const game_message_header& msg)
{
	for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
	{
		game_server_client* playerClient = game_server_get_client_data(server, slot.players[playerID].client);
		if (playerClient == nullptr || playerClient->type != game_server_client::TYPE::GAME_CLIENT) continue;

		playerClient->game_client_send_message(msg);
	}
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
        if (slot.connection_count == 0) slot.lobby->countdown_active = false;
    }

    server.logf("MATCH", LOG_NORMAL, "Detached client %d from match slot %d. Freed player index = %hu.", client_handle.value, slot_index, freedSlot);
    return true;
}

// BEGIN SLOT LIFECYCLE FUNCTIONS

bool game_server_match_slot_init(game_server& server, mem_arena& slot_mem, ui8 slot_index)
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

bool game_server_match_slot_reset(game_server& server, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	if (slot.state != MATCH_SLOT_STATE::PLAY_ENDED && slot.state != MATCH_SLOT_STATE::AWAITING_CLEANUP)
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

// Handles a game message from the client. Returns whether the message was handled.
bool game_server_match_slot_handle_message(game_server& server, game_server_client::client_handle client_handle, const game_message_header &msg)
{
    game_server_client* playerClient = game_server_get_client_data(server, client_handle);
    if (playerClient == nullptr || playerClient->type != game_server_client::TYPE::GAME_CLIENT) return false;
    if (playerClient->game_client.match_slot >= server.init_params.match_slot_count) return false;

    match_slot_index slotIndex = playerClient->game_client.match_slot;
    match_slot& slot = server.match_slots[slotIndex];

    switch(msg.message_type)
    {
    case GAME_MESSAGE_TYPE::CLIENT_PLACEMENT_PICK:
        match_slot_handle_message_client_placement_pick(server, slotIndex, playerClient, msg);
        return true;
    case GAME_MESSAGE_TYPE::CLIENT_TICK:
        match_slot_handle_message_client_tick(server, slotIndex, playerClient, msg);
        return true;
    case GAME_MESSAGE_TYPE::SIGNAL_LEAVE_LOBBY:
        match_slot_handle_message_leave_lobby(server, slotIndex, playerClient);
        return true;
    case GAME_MESSAGE_TYPE::LOBBY_ADMIN_START_MATCH:
    case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_PLAYER_COUNT:
    case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_MAP_SIZE:
    case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_SEED:
    case GAME_MESSAGE_TYPE::LOBBY_ADMIN_SET_TICK_RATE:
        match_slot_handle_message_lobby_admin(server, slotIndex, playerClient, msg);
        return true;
    default:
        return false;
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
		case MATCH_SLOT_STATE::IN_PLAY:
			game_server_tick_match_slot_ongoing(server, slotIndex);
			break;
        case MATCH_SLOT_STATE::PLAY_ENDED:
            // ... TODO: Wait until match results have been transmitted to all player & master server.

            // FALLTHROUGH
        case MATCH_SLOT_STATE::AWAITING_CLEANUP:
            // Reset slot.
            game_server_match_slot_reset(server, slotIndex);
            break;
		default:
			// Do nothing.
			break;
		}
	}
}

// END MATCH SLOT SYSTEM IMPLEMENTATION
