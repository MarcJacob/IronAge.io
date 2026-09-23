// Game server core clients management implementation.

#include "core.h"

#include "game_server.h"
#include "game_server/game_server_platform.h"
#include "game_server_clients.h"

// BEGIN CLIENT TABLE SYSTEM IMPLEMENTATION

static constexpr const char* CLIENTS_COMPONENT_NAME = "Clients";

struct game_server_clients_table
{
	game_server_client* _client_buff; // Buffer of client data.
	ui16 _client_count; // Number of connected clients.
	ui16 _client_capacity; // Maximum of concurrent client connections.

	// Event handlers

	// Allows a single function to react to a client of a certain type to react to that client being dropped / deleted for any reason.
	on_client_disconnected_fn on_client_disconnected_handlers[(i32)game_server_client::TYPE::TYPE_COUNT];
};

// Initializes the clients table associated with the server.
// max_client_count specifies the maximum amount of concurrent client connections supported by the server.
void game_server_init_clients_table(game_server& server, ui16 max_client_count)
{
	server.log(CLIENTS_COMPONENT_NAME, "Initializing client connections table...");

	server.client_table = server.main_memory.alloc<game_server_clients_table>();
	ASSERT(server.client_table != nullptr);

	server.client_table->_client_buff = server.main_memory.alloc<game_server_client>(max_client_count);
	ASSERT(server.client_table->_client_buff != nullptr);

	server.client_table->_client_capacity = max_client_count;
	server.client_table->_client_count = 0;

	server.logf(CLIENTS_COMPONENT_NAME, LOG_SUCCESS, "Client connections table initialized. Capacity = %d", server.client_table->_client_capacity);
}

game_server_client* game_server_clients_register_connection(game_server& server, game_server_platform::in_connection& connection_info)
{
	// Look for a FREE client slot in the table and create a new client there from the provided platform connection info.
	// TODO(Marc): Intermediate disconnection state, reconnection system.

	ASSERT(server.client_table != nullptr);
	game_server_clients_table& clientsTable = *server.client_table;

	for (ui16 clientIndex = 0; clientIndex < clientsTable._client_capacity; clientIndex++)
	{
		game_server_client& client = clientsTable._client_buff[clientIndex];
		if (client.state == game_server_client::STATE::FREE)
		{
			client.state = game_server_client::STATE::ONLINE;
			client.connection_info = {
				.last_known_address = connection_info.address,
				.last_known_port = connection_info.port,
				.connection_handle = (ui64)connection_info.platform_handle,
			};
			client.type = game_server_client::TYPE::UNKNOWN;

			client.handle._table_index = clientIndex;
			client.handle._create_time_ms = server.time_ms;

			server.logf(CLIENTS_COMPONENT_NAME, LOG_TYPE::LOG_SUCCESS, 
				"Registered new client connection from platform connection handle %d. Index = %hu, Handle = %d.",
				connection_info.platform_handle, clientIndex, client.handle.value);

			clientsTable._client_count++;

			return &client;
		}
	}

	return nullptr; // Failed to find room. Let the caller handle the consequences.
}

void game_server_clients_connection_lost(game_server& server, game_server_platform::net_connection_handle connection_handle)
{
	ASSERT(server.client_table != nullptr);
	game_server_clients_table& clientsTable = *server.client_table;	
	
	// Look for a client with a matching connection handle and get rid of them by setting them to FREE state.
	// TODO(Marc): Intermediate disconnection state, reconnection system.

	for (ui16 clientIndex = 0; clientIndex < clientsTable._client_capacity; clientIndex++)
	{
		game_server_client& client = clientsTable._client_buff[clientIndex];
		if (client.state != game_server_client::STATE::FREE && client.connection_info.connection_handle == connection_handle)
		{
			server.logf(CLIENTS_COMPONENT_NAME, LOG_TYPE::LOG_WARNING,
				"Lost connection with client %d. Removing from active client connections...", client.handle.value);

			// Free client after calling relevant event handler.
			if (clientsTable.on_client_disconnected_handlers[(i32)client.type] != nullptr)
			{
				clientsTable.on_client_disconnected_handlers[(i32)client.type](server, client);
			}

			// Completely reset client data.
			client = {};

			clientsTable._client_count--;
		}
	}
}

game_server_client* game_server_get_client_data(game_server& server, game_server_client::client_handle handle)
{
	ASSERT(server.client_table != nullptr);
	ASSERT(handle._table_index < server.client_table->_client_capacity);

	game_server_clients_table& clientsTable = *server.client_table;	
	
	game_server_client& client = clientsTable._client_buff[handle._table_index];
	if (client.state != game_server_client::STATE::FREE 
		&& client.handle.value == handle.value) return &client;

	return nullptr; // Handle was stale.
}

void clients_table_register_event_handler_client_connection_lost(game_server_clients_table& table, game_server_client::TYPE client_type, on_client_disconnected_fn handler)
{
	ASSERT(table.on_client_disconnected_handlers[(i32)client_type] == nullptr);
	table.on_client_disconnected_handlers[(i32)client_type] = handler;
}

/// END CLIENT TABLE SYSTEM IMPLEMENTATION
