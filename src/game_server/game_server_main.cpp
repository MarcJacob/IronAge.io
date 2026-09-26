// Main implementation file for the Game Server code.
// Must be linked or compiled into whatever platform layer is used.

// Main "chaining" of game server platform and game server internals.
#include "game_server/game_server_platform.h"
#include "game_server.h"

// Match slots system.
#include "match_slots.h"

// Unity-compile the Game Common code into the server.
#include "../game_common/game_common_main.cpp"

// Unity-compile sub-components.
#include "game_server_clients.cpp"
#include "game_server_http.cpp"

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
	match_start(slot.slot_memory, server.uptime_ms, slot.match_params, match);

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

// END MATCH SLOT SYSTEM IMPLEMENTATION

// Unknown client handling.

static constexpr ui32 UNKNOWN_CLIENT_READ_BUFFER_SIZE = 16; // Enough to recognize a request line's method. Anything beyond stays on the platform.
static constexpr time_ms UNKNOWN_CLIENT_TIMEOUT_MS = 1000; // Unknown clients still unrecognized after this long get dropped.

void game_server_process_unknown_client(game_server& server, game_server_client& client)
{
	// Receive some data from the client connection in a small local buffer.
	ui8 readBuffer[UNKNOWN_CLIENT_READ_BUFFER_SIZE] = {0};
	ui32 receivedBytes = game_server_client_receive_message(server, client.handle, readBuffer, sizeof(readBuffer) - 1);

	if (receivedBytes > 0)
	{
		client.unknown.traffic_size += receivedBytes;

		// Ask HTTP server if it recognized the bytes to connect as a http request. If it does, register the client with http server and set its type.
		if (http_server_try_accept_client(server, client, readBuffer, receivedBytes)) return;
	}

	// If the connection is still unrecognized and has lasted more than a second, drop it.
	if (server.uptime_ms - client.connected_at_ms > UNKNOWN_CLIENT_TIMEOUT_MS)
	{
		server.logf("Clients", LOG_WARNING, "Dropping unrecognized client %d after %llu ms (received %llu bytes).",
			client.handle.value, server.uptime_ms - client.connected_at_ms, client.unknown.traffic_size);
		game_server_client_drop(server, client.handle);
	}
}

// BEGIN GAME SERVER MAIN FUNCTIONS

game_server* game_server_init(game_server_platform& platform, game_server_init_params& init_params, ui8* memory, ui64 memory_size)
{
	ASSERT_MSG(memory != nullptr && memory_size > GiB(2), "Game server requires at least 2 Gibibytes of memory !");

	// Check init params.
	if (init_params.web_root == nullptr || ia_str_len(init_params.web_root) == 0)
	{
		platform.log(LOG_ERROR, "HTTP Server requires a valid web root folder, relative to the platform resources path. Aborting.");
		return nullptr;
	}
	if (init_params.web_files == nullptr || init_params.web_file_count == 0)
	{
		platform.log(LOG_ERROR, "HTTP Server requires at least one file to serve. Aborting.");
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

	// Initialize HTTP Server.

	newServer->http = http_server_init(*newServer);
	http_server_load_files(*newServer);

	// Setup event handler for HTTP server to clean resources tied to non-game-clients losing connection.
	clients_table_register_event_handler_client_connection_lost(*newServer->client_table, game_server_client::TYPE::NON_GAME_CLIENT,
		http_server_on_client_disconnected);

	return newServer;
}

void game_server_test_mode_tick(game_server& server)
{	
	// TEST: Run the shared test scenario in its own arena allocated from main memory,
	// dump the resulting match state to specified file and shut down.

	ASSERT(server.platform != nullptr);
	game_server_platform& platform = *server.platform;

	server.log("TEST", "Running in test scenario mode.\nRunning test scenario match...");

	game_match_start_params scenario_params = match_test_scenario_get_params();

	mem_arena scenario_memory = mem_arena_create_sub(server.main_memory, match_get_required_mem(scenario_params));
	game_match* scenario_match = match_run_test_scenario(scenario_memory);
	ASSERT(scenario_match != nullptr);

	if (server.init_params.test_scenario_dump_filename == nullptr)
	{
		server.log("TEST", "No dump file specified. Going straight to shutdown.");
		platform.shutdown(0);
	}

	server.logf("TEST", "Dumping scenario match end state to file \"%s\".", server.init_params.test_scenario_dump_filename);

	match_dump_stream dump_stream = { };
	ui64 dumpSize = match_dump_gamestate(*scenario_match, dump_stream);

	struct dump_state_data
	{
		ui8* dump_mem;
		ui64 dump_size;
	} dump_state = {0};

	if (dumpSize > 0)
	{
		dump_state.dump_mem = server.main_memory.alloc<ui8>(dumpSize);
		ASSERT(dump_state.dump_mem != nullptr);

		auto dump_func = [](void* dump_state, ui8* bytes, ui64 byte_count)
			{
				dump_state_data& state = *(dump_state_data*)(dump_state);

				ia_memcpy(state.dump_mem + state.dump_size, bytes, byte_count);
				state.dump_size += byte_count;
			};

		dump_stream.dump_func = dump_func;
		dump_stream.state = &dump_state;
		match_dump_gamestate(*scenario_match, dump_stream);

		// Write the snapshot so it can be compared with the one simulated by the web client.
		if (!platform.write_resource_file(server.init_params.test_scenario_dump_filename, dump_state.dump_mem, dump_state.dump_size))
		{
			server.log("TEST", LOG_ERROR, "Failed to write native snapshot file.");
		}
		else
		{
			server.log("TEST", LOG_SUCCESS, "Match ending state dumped successfully.");
		}
	}

	server.log("TEST", "Shutting down...");
	platform.shutdown(0);
}

void game_server_tick(game_server& server, time_ms platform_time_ms)
{
	// ... for convenience.
	ASSERT(server.platform != nullptr);
	game_server_platform& platform = *server.platform;

	server.delta_ms = platform_time_ms - server.uptime_ms;
	server.uptime_ms = platform_time_ms;

	// Run in test mode if configured to do so.
	// NOTE(Marc): Having this here is ugly. Need to design a proper "test mode" alternative server implementation.
	if (server.init_params.run_test_scenario)
	{
		game_server_test_mode_tick(server);
		return;
	}

	// Query platform for closed connections, and signal the clients subsystem about them.
	{
		static constexpr ui16 CLOSED_CONNECTIONS_BUFF_SIZE = 32;
		game_server_platform::net_connection_handle closedConnectionsBuffer[CLOSED_CONNECTIONS_BUFF_SIZE];
		ui16 closedConnectionsCount = server.platform->net_query_closed_connections(closedConnectionsBuffer, CLOSED_CONNECTIONS_BUFF_SIZE);

		for (ui16 closedConnectionCount = 0; closedConnectionCount < closedConnectionsCount; closedConnectionCount++)
		{
			game_server_platform::net_connection_handle& closedConnectionHandle = closedConnectionsBuffer[closedConnectionCount];
			clients_table_on_connection_lost(server, closedConnectionHandle);
		}
	}

	// Query platform for new connections, and register them into the clients subsystem.
	{
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
				platform.net_close_connection(newConnection.platform_handle);
			}
		}
	}

	// Process UNKNOWN type client connections.
	game_server_client_for_each_of_type(server, game_server_client::TYPE::UNKNOWN, game_server_process_unknown_client);

	// Serve the web client bundle over HTTP on all platform connections.
	http_server_tick(server);

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
			
			msPerTick = (1000 / slot.match_params.tick_rate);

			// TODO: Avoid starvation by budgeting ticking time on each slot and ticking each once evenly instead of catching each one up then the next.
			nextTickTimeMs = slot.match.last_tick_time + msPerTick;
			while (nextTickTimeMs < server.uptime_ms)
			{
				// TODO: Input system based on received network messages.
				match_tick_commands tickCommands = {};
				match_tick(*slot.match.match_ptr, tickCommands);

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
