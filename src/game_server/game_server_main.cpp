// Main implementation file for the Game Server code.
// Must be linked or compiled into whatever platform layer is used.

// Main "chaining" of game server platform and game server internals.
#include "game_server/game_server_platform.h"
#include "game_server.h"

// Match slots system.
#include "match_slots.h"
#include "game_common/game_match.h"
#include "game_common/game_commands.h"
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

	slot.match.match_ptr = slot.slot_memory.alloc<game_match>();
	slot.match.last_tick_time = server.uptime_ms;

	game_match& match = *slot.match.match_ptr;
	match_start(slot.slot_memory, server.uptime_ms, *slot.match_params, match);

	slot.state = MATCH_SLOT_STATE::MATCH_ONGOING;

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

	if (slot.players == nullptr)
	{
		slot.players = slot.slot_memory.alloc<match_player>(slot.match_params->player_count);
		ASSERT(slot.players != nullptr);
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

	ui8 msg_scratch_buff[2048];
	mem_arena msg_scratch_mem = mem_arena_create(msg_scratch_buff, sizeof(msg_scratch_buff));

	// Send JOIN MATCH message to newly-attached game client.

	// Allocate message, adding extra data size of the match parameters as extra bytes.
	game_match_start_params* matchParams = slot.match_params;
	ASSERT(matchParams != nullptr);

	game_message_header* joinMsgHeader = build_game_message(GAME_MESSAGE_TYPE::MATCH_JOINED, msg_scratch_mem, matchParams->extra_data_size);
	ASSERT(joinMsgHeader);

	auto& joinMsgPayload = joinMsgHeader->get_payload_ref<game_message_payload_match_joined>();
	joinMsgPayload = {};

	joinMsgPayload.controlled_player_id = out_player_index;
	
	if (server.match_slots[slot_index].state == MATCH_SLOT_STATE::IN_LOBBY)
	{
		joinMsgPayload.join_tick = 0;
	}
	else
	{
		joinMsgPayload.join_tick = slot.match.match_ptr->tick;
	}

	// Copy start params.
	ia_memcpy(&joinMsgPayload.match_start_params, slot.match_params, sizeof(game_match_start_params) + matchParams->extra_data_size);

	// Send message !
	client.game_client_send_message(*joinMsgHeader);

	return true;
}

void game_server_tick_match_slots(game_server& server)
{
	// Manage match slots.
	for (ui8 slotIndex = 0; slotIndex < server.init_params.match_slot_count; slotIndex++)
	{
		match_slot& slot = server.match_slots[slotIndex];
		time_ms nextTickTimeMs = 0;
		ui64 msPerTick = 0;

		switch (slot.state)
		{
		case MATCH_SLOT_STATE::MATCH_ONGOING:
			// Ongoing match tick logic.
			
			msPerTick = (1000 / slot.match_params->tick_rate);

			// TODO: Avoid starvation by budgeting ticking time on each slot and ticking each once evenly instead of catching each one up then the next.
			nextTickTimeMs = slot.match.last_tick_time + msPerTick;
			while (nextTickTimeMs < server.uptime_ms)
			{
				// TODO: Input system based on received network messages.
				match_tick(*slot.match.match_ptr, {});

				// TEST: End match after 2500 ticks.
				if (slot.match.match_ptr->tick == 2500)
				{
					game_server_end_match_slot(server, slotIndex);
					break;
				}

				slot.match.last_tick_time = nextTickTimeMs;
				nextTickTimeMs += msPerTick;
			}
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

	constexpr ui64 MEM_PER_SLOT = MiB(2); // TEMP(Marc): Just keep this number above the required memory for a match or a lobby, whichever is larger.
	for (ui8 matchSlotIndex = 0; matchSlotIndex < init_params.match_slot_count; matchSlotIndex++)
	{
		mem_arena slot_mem = mem_arena_create_sub(newServer->main_memory, MEM_PER_SLOT);	
		game_server_init_match_slot(*newServer, slot_mem, matchSlotIndex);
	}

	// In debug, run checks over commands & game messages.
#ifndef NDEBUG
	TEST_COMMAND_SIZES_CHECK();
	TEST_MESSAGE_TYPE_SIZES_CHECK();
#endif

	// Initialize Web Server.

	newServer->web = web_server_init(*newServer);
	web_server_load_files(*newServer);

	// Setup event handler for Web server to clean resources tied to non-game-clients losing connection.
	clients_table_register_event_handler_client_connection_lost(*newServer->client_table, web_server_on_client_disconnected);

	// TEMP(Marc): Until we have a full working match slot lifecycle with join request messages from clients, we just start up match slot 0.
	game_server_open_lobby(*newServer, 0);
	{
		// .. Get parameters from test scenario.
		newServer->match_slots[0].match_params = match_test_scenario_get_params(newServer->match_slots[0].slot_memory);
	}

	game_server_start_match_slot(*newServer, 0);

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
