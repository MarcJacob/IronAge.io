// Main implementation file for the Game Server code.
// Must be linked or compiled into whatever platform layer is used.

// Main "chaining" of game server platform and game server internals.
#include "core/assert.h"
#include "game_server/game_server_platform.h"
#include "game_server.h"

// Match slots system.
#include "match_slots/match_slots.h"

// Clients table system
#include "game_server_clients.h"

// Game Messages protocol
#include "game_common/game_messages.h"

// Unity-compile sub-components.
#include "game_server_clients.cpp"
#include "web_server/web_server.cpp"
#include "match_slots/game_server_match_slots.cpp"

// Unity-compile the Game Common code into the server.
#include "../game_common/game_common_main.cpp"

#if _DEBUG
#include "tests/game_server_tests.cpp"
#endif

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
    // @TEST(Marc): Any Game Client not attached to a match gets attached to one.
	if (client.game_client.match_slot == INVALID_MATCH_SLOT_INDEX)
	{
		ui16 playerIndex = 0;
		if (game_server_slot_attach_client(server, 0, client.handle, playerIndex))
		{
			server.logf("MATCH", LOG_SUCCESS, "Attached client %d to match slot 0 as player %d.", client.handle.value, playerIndex);
		}
	}

    // Handle message reception.
    ui64 receivedBytes;
    const void* receptionBuffer = client.game_client_get_messages_reception_buffer(receivedBytes);
    
    ui64 readIndex = 0;
    while(readIndex < receivedBytes)
    {
        game_message_header* msgPtr = (game_message_header*)((ui8*)receptionBuffer + readIndex);
        ui64 totalSize = sizeof(game_message_header) + msgPtr->payload_size;

        if (totalSize == 0 || readIndex + totalSize > receivedBytes)
        {
            server.logf("CLIENTS", LOG_ERROR, "Received corrupted Game Message on client handle %d. Dropping the rest of the buffer...", client.handle);
            break;
        }

        // Just go through a list of known message handlers until one of them says they've handled it, then try our own handlers.
        
        bool handled = game_server_match_slot_handle_message(server, client.handle, *msgPtr);
        // || ... (More handlers...)

        if (!handled)
        {
            // Top-level handling
            // ...
        }

        readIndex += totalSize;
    }

    client.game_client_clear_reception_buffer();
}

// BEGIN GAME SERVER PROGRAM FUNCTIONS

#define GAME_SERVER_PROGRAM_EXPORT extern "C" __declspec(dllexport)

// Implementation of global assertion handler declared in assert.h
// Set by whoever loads the program.
_ASSERTION_HANDLER* _ASSERTION_HANDLER_PTR = nullptr;

GAME_SERVER_PROGRAM_EXPORT game_server* game_server_init(game_server_platform& platform, game_server_init_params& init_params, ui8* memory, ui64 memory_size);
GAME_SERVER_PROGRAM_EXPORT void game_server_tick(game_server& server, time_ms platform_time_ms);
GAME_SERVER_PROGRAM_EXPORT void game_server_stop(game_server& server);

GAME_SERVER_PROGRAM_EXPORT void game_server_load_program(game_server_program& program, _ASSERTION_HANDLER* assertion_handler);
GAME_SERVER_PROGRAM_EXPORT void game_server_on_program_unloaded();

// Called by platform anytime the server program is loaded, independently of whether the server
// has been initialized before.
GAME_SERVER_PROGRAM_EXPORT void game_server_load_program(game_server_program& program, _ASSERTION_HANDLER* assertion_handler)
{
    _ASSERTION_HANDLER_PTR = assertion_handler;

    // Fill in program.
    program.init_func = game_server_init;
    program.tick_func = game_server_tick;
    program.stop_func = game_server_stop;

    program.on_unload_func = game_server_on_program_unloaded;

    // If the server already exists, this is a hot reload: resources may have changed on disk.
    if (program._game_server_ptr != nullptr)
    {
        program._game_server_ptr->reload_resources = true;
    }
}

// Called right before the server program is unloaded, independently of whether the server is actually shutting down / has shut down.
GAME_SERVER_PROGRAM_EXPORT void game_server_on_program_unloaded()
{
    // ...
}

static constexpr ui8 MAX_RESOURCE_FILE_COUNT = 255;

// Lists the platform's resource files into the server's (already allocated) resource files buffer.
static void game_server_discover_resources(game_server& server)
{
	server.resource_file_count = server.platform->list_resource_files("*", server.resource_files, MAX_RESOURCE_FILE_COUNT);

	// TEST: List all discovered resource files.
	server.log("Discovered resource files:");
	for (ui8 resourceFileIndex = 0; resourceFileIndex < server.resource_file_count; resourceFileIndex++)
	{
		server.logf(LOG_NORMAL, "%s", (ia_string_view)server.resource_files[resourceFileIndex]);
	}
}

GAME_SERVER_PROGRAM_EXPORT game_server* game_server_init(game_server_platform& platform, game_server_init_params& init_params, ui8* memory, ui64 memory_size)
{
	ASSERT_MSG(memory != nullptr && memory_size > GiB(2), "Game server requires at least 2 Gibibytes of memory !");

	// Check init params.
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

	// Discover and pre-load all resource files.
	newServer->resource_files = newServer->main_memory.alloc<game_server_resource_path>(MAX_RESOURCE_FILE_COUNT);
	ASSERT_MSG(newServer->resource_files != nullptr, "Not enough server memory for the resource files list.");
	game_server_discover_resources(*newServer);

	// Initialize clients table subsystem.
	if (init_params.max_client_count == 0)
	{
		newServer->logf(LOG_ERROR, "Game server set to start with 0 supported client connections. This is currently not supported. Aborting.");
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
#endif

	// Initialize Web Server.

	newServer->web = web_server_init(*newServer);
	web_server_reload_files(*newServer);

	// TEMP(Marc): Until we have a full working match slot lifecycle with join request messages from clients, just set slot 0 to lobby and redirect all players there.	
	{
		// .. Get parameters from test scenario.
		newServer->match_slots[0].match_params = match_test_scenario_get_params(
                newServer->match_slots[0].slot_memory, (ui32)(iptr)&newServer, 2);
		game_server_open_lobby(*newServer, 0);
	}

	newServer->log(LOG_SUCCESS, "Game Server initialization successful.\n");
	return newServer;
}

void game_server_query_new_connections(game_server& server)
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

void game_server_query_closed_connections(game_server& server)
{
	// Query platform for closed connections, and signal the clients subsystem about them.
	static constexpr ui16 CLOSED_CONNECTIONS_BUFF_SIZE = 32;
	game_server_platform::net_connection_handle closedConnectionsBuffer[CLOSED_CONNECTIONS_BUFF_SIZE];
	ui16 closedConnectionsCount = server.platform->net_query_closed_connections(closedConnectionsBuffer, CLOSED_CONNECTIONS_BUFF_SIZE);

	for (ui16 closedConnectionIndex = 0; closedConnectionIndex < closedConnectionsCount; closedConnectionIndex++)
	{
		for (ui16 clientIndex = 0; clientIndex < server.client_table->_client_capacity; clientIndex++)
		{
			game_server_client& client = server.client_table->_client_buff[clientIndex];
			if (client.type == game_server_client::TYPE::NONE
				|| client.connection_info.connection_handle != closedConnectionsBuffer[closedConnectionIndex]) continue;

            // Signal relevant subsystems.
            web_server_on_client_disconnected(server, client);

            // Remove client from clients table.
            clients_table_remove_client(server, client.handle);
		}
	}
}

GAME_SERVER_PROGRAM_EXPORT void game_server_tick(game_server& server, time_ms platform_time_ms)
{
	ASSERT(server.platform != nullptr);
	game_server_platform& platform = *server.platform;

	server.delta_ms = platform_time_ms - server.uptime_ms;
	server.uptime_ms = platform_time_ms;

	// Reload resources if flagged (set on server program hot reload).
	if (server.reload_resources)
	{
		server.reload_resources = false;
		game_server_discover_resources(server);
		web_server_reload_files(server);
	}

	// Query new & closed connections.
	game_server_query_new_connections(server);
    game_server_query_closed_connections(server);

	// Process UNKNOWN type client connections.
	game_server_client_for_each_of_type(server, game_server_client::TYPE::UNKNOWN, game_server_process_unknown_client);

	// Tick sub-components
	{
		// Tick web server.
		web_server_tick(server);

		// ...
	}

	// Process GAME_CLIENT type client connections.
    // Will handle game message reception & dispatch.
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

GAME_SERVER_PROGRAM_EXPORT void game_server_stop(game_server& server)
{
	server.log(LOG_WARNING, "Shutting down...");
	// TODO(Marc): Shut down work / checks to be done here (gracefully end connections / matches).
	// ...

	server.log(LOG_SUCCESS, "Shutdown complete."); // Log the fact the server did everything it wanted to do before shutting down.
}

// END GAME SERVER MAIN FUNCTIONS
