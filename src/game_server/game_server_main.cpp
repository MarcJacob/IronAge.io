// Main implementation file for the Game Server code.
// Must be linked or compiled into whatever platform layer is used.

// Main "chaining" of game server platform and game server internals.
#include "game_server/game_server_platform.h"
#include "game_server.h"

// Match slots system.
#include "match_slots.h"

#include "game_common/match/match.h"
#include "game_common/match/commands.h"
#include "game_common/game_messages.h"

// Clients table symbols
#include "game_server_clients.h"

// Unity-compile the Game Common code into the server.
#include "../game_common/game_common_main.cpp"

// Unity-compile sub-components.
#include "game_server_clients.cpp"
#include "web_server/web_server.cpp"

#if _DEBUG
#include "tests/game_server_tests.cpp"
#endif

// BEGIN MATCH SLOT SYSTEM IMPLEMENTATION

bool game_server_init_match_slot(game_server& server, mem_arena& slot_mem, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	if (slot.state != MATCH_SLOT_STATE::UNINITIALIZED)
	{
		server.logf("MATCH", LOG_ERROR, "Match slot index %d is already initialized (current state = %d).", slot_index, slot.state);
		return false;
	}

	server.logf("MATCH", "Initializing server match slot index %d.", slot_index);

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

	server.logf("MATCH", "Opening lobby in match slot %d.", slot_index);

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

	server.logf("MATCH", "Starting match in match slot %d.", slot_index);

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

	ui8 msg_scratch_buff[2048];
	mem_arena msg_scratch_mem = mem_arena_create(msg_scratch_buff, sizeof(msg_scratch_buff));

	// Allocate message, adding extra data size of the match parameters as extra bytes.
	game_match_start_params* matchParams = slot.match_params;
	ASSERT(matchParams != nullptr);

	game_message_header* joinMsgHeader = build_game_message(GAME_MESSAGE_TYPE::MATCH_JOINED, msg_scratch_mem, matchParams->extra_data_size);
	ASSERT(joinMsgHeader);

	auto& joinMsgPayload = joinMsgHeader->get_payload_ref<game_message_payload_match_joined>();
	joinMsgPayload = {};
	// Copy start params.
	ia_memcpy(&joinMsgPayload.match_start_params, slot.match_params, sizeof(game_match_start_params) + matchParams->extra_data_size);

	// Allocate room for AI players in match memory.
	// TODO(Marc): Prepass to count how many human players there are so we never over-allocate.
	mem_arena AIPlayersMem = mem_arena_create_sub(slot.slot_memory, ia_min(256, matchParams->player_count) * sizeof(AI_player_state));
	AIPlayersMem.clear();

	for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
	{
		match_player& playerSlot = slot.players[playerID];
		const game_server_client* playerClient = game_server_get_client_data(server, playerSlot.client);
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

		playerSlot.bIsClientPlayer = true;

		joinMsgPayload.controlled_player_id = playerID;
		joinMsgPayload.join_tick = 0;

		// Send message !
		playerClient->game_client_send_message(*joinMsgHeader);
	}

	// Link AI players buffer.
	slot.match.ai_players = (AI_player_state*)AIPlayersMem.mem_start;

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

	server.logf("MATCH", "Ending match in match slot %d.", slot_index);

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

	server.logf("MATCH", "Resetting server match slot index %d.", slot_index);

	// Zero out the slot and set it back to waiting. Conserve only its memory.

	mem_arena slot_mem = slot.slot_memory;

	slot = {};
	slot.state = MATCH_SLOT_STATE::WAITING;
	
	// Reset memory.
	slot.slot_memory = slot_mem;
	slot.slot_memory.allocated_count = 0; // TODO(Marc): A "clear" function pointer that can be adapted to allocation strategy will be needed at some point.
	ia_memzero(slot.slot_memory.mem_start, slot.slot_memory.mem_size);

	// From there the slot can be put into lobby mode to start accepting players, or directly have its parameters set and match started.

	return true;
}

bool game_server_slot_attach_client(game_server& server, ui8 slot_index, game_server_client::client_handle client_handle, ui16& out_player_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);
	match_slot& slot = server.match_slots[slot_index];

	ASSERT(slot.state == MATCH_SLOT_STATE::IN_LOBBY
		|| slot.state == MATCH_SLOT_STATE::MATCH_ONGOING);

	const game_server_client* clientPtr = game_server_get_client_data(server, client_handle);
	ASSERT(clientPtr != nullptr);	
	
	const game_server_client& client = *clientPtr;
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
	if (connectionCount < 1) return; // Wait until a player connects.

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

		ui8 tick_commands_scratch[4096];
		mem_arena tick_commands_scratch_mem = mem_arena_create(tick_commands_scratch, sizeof(tick_commands_scratch));

		match_tick_commands_builder tickCommandsBuilder = {};
		tickCommandsBuilder.target_mem = &tick_commands_scratch_mem;
		ASSERT(tickCommandsBuilder.init());

		// Gather inputs for this tick.
		// Do so by going over every player controlled by a Game Client and checking them for CLIENT_TICK messages.
		for (match_player_id playerID = 0; playerID < slot.match_params->player_count; playerID++)
		{
			const game_server_client* playerClient = game_server_get_client_data(server, slot.players[playerID].client);
			if (playerClient == nullptr || playerClient->type != game_server_client::TYPE::GAME_CLIENT) continue;

			game_message_header* msgHeader;
			if (!playerClient->game_client_peek_message(msgHeader)) continue;

			if (msgHeader->message_type == GAME_MESSAGE_TYPE::CLIENT_TICK)
			{
				const auto& payload = msgHeader->get_payload_ref<game_message_payload_client_tick>();
				if (slot.match.match_ptr->tick - payload.emit_tick < slot.match_params->tick_rate) // Don't take command into account if it was emitted on too old a tick.
				{
					tickCommandsBuilder.push_validated_sequence(playerID, *slot.match.match_ptr, payload.commands);
				}
			}
			playerClient->game_client_consume_message();
		}

		// Gather AI input.
		for (ui16 aiPlayerIndex = 0; aiPlayerIndex < slot.match.ai_player_count; aiPlayerIndex++)
		{
			AI_player_state& aiState = slot.match.ai_players[aiPlayerIndex];
			AI_output_commands(*slot.match.match_ptr, aiState, tickCommandsBuilder);
		}

		match_tick_commands& tickCommands = *tickCommandsBuilder._tick_commands_start;

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
			const game_server_client* playerClient = game_server_get_client_data(server, slot.players[playerID].client);
			if (playerClient == nullptr || playerClient->type != game_server_client::TYPE::GAME_CLIENT) continue;

			playerClient->game_client_send_message(*tickMsgHeader);
		}

		// Perform server-side match tick.
		match_tick(*slot.match.match_ptr, tickCommands);

		// Reclaim the throwaway per-tick command memory now that it's been sent and applied.
		slot.slot_memory.allocated_count = slotMemoryAllocated;

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

// Unknown client handling.

static constexpr ui32 UNKNOWN_CLIENT_READ_BUFFER_SIZE = 16; // Enough to recognize a request line's method. Anything beyond stays on the platform.
static constexpr time_ms UNKNOWN_CLIENT_TIMEOUT_MS = 1000; // Unknown clients still unrecognized after this long get dropped.

void game_server_process_unknown_client(game_server& server, game_server_client& client)
{
	// Receive some data from the client connection in a small local buffer.
	ui8 readBuffer[UNKNOWN_CLIENT_READ_BUFFER_SIZE] = {0};
	ui32 receivedBytes = game_server_client_receive_net_bytes(server, client.handle, readBuffer, sizeof(readBuffer) - 1);

	if (receivedBytes > 0)
	{
		client.unknown.traffic_size += receivedBytes;

		// Ask Web server if it recognized the bytes to connect as a http request. If it does, register the client with the web server and set its type.
		if (web_server_try_accept_client(server, client, readBuffer, receivedBytes)) return;
	}

	// If the connection is still unrecognized and has lasted more than a second, drop it.
	if (server.uptime_ms - client.connected_at_ms > UNKNOWN_CLIENT_TIMEOUT_MS)
	{
		server.logf("Clients", LOG_WARNING, "Dropping unrecognized client %d after %llu ms (received %llu bytes).",
			client.handle.value, server.uptime_ms - client.connected_at_ms, client.unknown.traffic_size);
		game_server_client_drop(server, client.handle);
	}
}

// GAME_CLIENT type client handling. Top-level Game Client management.

void game_server_tick_game_client(game_server& server, game_server_client& client)
{
	if (!client.game_client.attached_to_match)
	{
		ui16 playerIndex = 0;
		if (game_server_slot_attach_client(server, 0, client.handle, playerIndex))
		{
			client.game_client.attached_to_match = true;
			server.logf("MATCH", LOG_SUCCESS, "Attached client %d to match slot 0 as player %d.", client.handle.value, playerIndex);
		}
	}
}

// BEGIN GAME SERVER MAIN FUNCTIONS

game_server* game_server_init(game_server_platform& platform, game_server_init_params& init_params, ui8* memory, ui64 memory_size)
{
	ASSERT_MSG(memory != nullptr && memory_size > GiB(2), "Game server requires at least 2 Gibibytes of memory !");

	// Check init params.
	if (init_params.web_root == nullptr || ia_str_len(init_params.web_root) == 0)
	{
		platform.log(LOG_ERROR, "Web Server requires a valid web root folder, relative to the platform resources path. Aborting.");
		return nullptr;
	}
	if (init_params.web_files == nullptr || init_params.web_file_count == 0)
	{
		platform.log(LOG_ERROR, "Web Server requires at least one file to serve. Aborting.");
		return nullptr;
	}
	if (init_params.match_slot_count == 0)
	{
		platform.log(LOG_ERROR, "Game Server requires at least one match slot to function. Aborting.");
		return nullptr;
	}

	// Allocate and initialize new game server at the start of memory.
	game_server* newServer = (game_server*)memory;
	newServer->platform = &platform;
	newServer->init_params = init_params;

	// Create main arena allocator for the server. 
	// It will be split into various static "Sections" that each hold the data needed by a feature of the server.
	newServer->main_memory = mem_arena_create(memory + sizeof(game_server), memory_size - sizeof(game_server));

	// Initialize clients table subsystem.
	if (init_params.max_client_count == 0)
	{
		newServer->logf(LOG_TYPE::LOG_ERROR, "Game server set to start with 0 supported client connections. This is currently not supported. Aborting.");
		return nullptr;
	}
	clients_table_init(*newServer, init_params.max_client_count);

	// Initialize all match slots.
	newServer->match_slots = newServer->main_memory.alloc<match_slot>(newServer->init_params.match_slot_count);

	constexpr ui64 MEM_PER_SLOT = MiB(64); // TEMP(Marc): Just keep this number above the required memory for a match or a lobby, whichever is larger.
	for (ui8 matchSlotIndex = 0; matchSlotIndex < init_params.match_slot_count; matchSlotIndex++)
	{
		mem_arena slot_mem = mem_arena_create_sub(newServer->main_memory, MEM_PER_SLOT);	
		game_server_init_match_slot(*newServer, slot_mem, matchSlotIndex);
	}

	// In debug, run checks over commands & game messages.
#ifndef NDEBUG
	TEST_COMMAND_SIZES_CHECK();
	TEST_MESSAGE_TYPE_SIZES_CHECK();

	// Math tests !!

	newServer->logf("sqrt(2.f) = %f", ia_sqrt(2.f));
	newServer->logf("sqrt(16.f) = %f", ia_sqrt(16.f));
	newServer->logf("sqrt(64.f) = %f", ia_sqrt(64.f));
	newServer->logf("sqrt(900.f) = %f", ia_sqrt(900.f));
	newServer->logf("sqrt(10000.f) = %f", ia_sqrt(10000.f));

	newServer->logf("rand(1.f, 2.f) = %f", ia_rand_range(1.f, 2.f));

#endif

	// Initialize Web Server.

	newServer->web = web_server_init(*newServer);
	web_server_load_files(*newServer);

	// Setup event handler for Web server to clean resources tied to non-game-clients losing connection.
	clients_table_register_event_handler_client_connection_lost(*newServer->client_table, web_server_on_client_disconnected);

	// TEMP(Marc): Until we have a full working match slot lifecycle with join request messages from clients, just set slot 0 to lobby and redirect all players there.	
	{
		// .. Get parameters from test scenario.
		newServer->match_slots[0].match_params = match_test_scenario_get_params(newServer->match_slots[0].slot_memory);	
		game_server_open_lobby(*newServer, 0);
	}

	return newServer;
}

void game_server_query_new_connections(game_server& server)
{
	// Query platform for closed connections, and signal the clients subsystem about them.
	static constexpr ui16 CLOSED_CONNECTIONS_BUFF_SIZE = 32;
	game_server_platform::net_connection_handle closedConnectionsBuffer[CLOSED_CONNECTIONS_BUFF_SIZE];
	ui16 closedConnectionsCount = server.platform->net_query_closed_connections(closedConnectionsBuffer, CLOSED_CONNECTIONS_BUFF_SIZE);

	for (ui16 closedConnectionIndex = 0; closedConnectionIndex < closedConnectionsCount; closedConnectionIndex++)
	{
		for (ui16 clientIndex = 0; clientIndex < server.client_table->_client_capacity; clientIndex++)
		{
			const game_server_client& client = server.client_table->_client_buff[clientIndex];
			if (client.type == game_server_client::TYPE::NONE
				|| client.connection_info.connection_handle != closedConnectionsBuffer[closedConnectionIndex]) continue;

			clients_table_on_connection_lost(server, client.handle);
		}
	}
}

void game_server_query_closed_connections(game_server& server)
{
	// Query platform for new connections, and register them into the clients subsystem.
	static constexpr ui16 NEW_CONNECTIONS_BUFF_SIZE = 32;
	game_server_platform::in_connection newConnectionsBuffer[NEW_CONNECTIONS_BUFF_SIZE];
	ui16 newConnectionsCount = server.platform->net_query_new_connections(newConnectionsBuffer, NEW_CONNECTIONS_BUFF_SIZE);

	for (ui16 newConnectionIndex = 0; newConnectionIndex < newConnectionsCount; newConnectionIndex++)
	{
		// Register new connection with clients table.
		game_server_platform::in_connection& newConnection = newConnectionsBuffer[newConnectionIndex];
		if (clients_table_register_new_connection(server, newConnection) == nullptr)
		{
			server.logf(LOG_TYPE::LOG_ERROR, "Out of room in the clients table (capacity = %d), dropping platform connection handle %d.",
				server.init_params.max_client_count, newConnection.platform_handle);
			server.platform->net_close_connection(newConnection.platform_handle);
		}
	}
}

void game_server_tick(game_server& server, time_ms platform_time_ms)
{
	ASSERT(server.platform != nullptr);
	game_server_platform& platform = *server.platform;

	server.delta_ms = platform_time_ms - server.uptime_ms;
	server.uptime_ms = platform_time_ms;

#if _DEBUG
	// Run in test mode if configured to do so.
	// NOTE(Marc): Having this here is ugly. Need to design a proper "test mode" alternative server implementation.
	if (server.init_params.run_test_scenario)
	{
		game_server_test_mode_tick(server);
		return;
	}
#endif

	// Query new & closed connections.
	game_server_query_new_connections(server), game_server_query_closed_connections(server);

	// Process UNKNOWN type client connections.
	game_server_client_for_each_of_type(server, game_server_client::TYPE::UNKNOWN, game_server_process_unknown_client);

	// Tick sub-components
	{
		// Tick web server.
		web_server_tick(server);

		// ...
	}

	// Process GAME_CLIENT type client connections.
	game_server_client_for_each_of_type(server, game_server_client::TYPE::GAME_CLIENT, game_server_tick_game_client);

	// Update / tick match slots.
	game_server_tick_match_slots(server);

	server.tick_count++;

	if (server.shutdown_triggered)
	{
		server.log(LOG_WARNING, "Shutdown requested from server code. Requesting platform shutdown...");
		platform.shutdown(0);
	}
}

void game_server_stop(game_server& server)
{
	server.log(LOG_WARNING, "Shutting down...");
	// TODO(Marc): Shut down work / checks to be done here (gracefully end connections / matches).
	// ...

	server.log(LOG_SUCCESS, "Shutdown complete."); // Log the fact the server did everything it wanted to do before shutting down.
}

// END GAME SERVER MAIN FUNCTIONS
