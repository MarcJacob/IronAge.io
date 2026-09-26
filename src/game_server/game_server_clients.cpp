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

	// Called when any non-UNKNOWN client gets disconnected.
	on_client_disconnected_fn on_client_disconnected_handlers[8];
	ui8 on_client_disconnected_handler_count;
};

// Initializes the clients table associated with the server.
// max_client_count specifies the maximum amount of concurrent client connections supported by the server.
void clients_table_init(game_server& server, ui16 max_client_count)
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

game_server_client* clients_table_register_new_connection(game_server& server, game_server_platform::in_connection& connection_info)
{
	// Look for a FREE client slot in the table and create a new client there from the provided platform connection info.
	// TODO(Marc): Intermediate disconnection state, reconnection system.

	ASSERT(server.client_table != nullptr);
	game_server_clients_table& clientsTable = *server.client_table;

	for (ui16 clientIndex = 0; clientIndex < clientsTable._client_capacity; clientIndex++)
	{
		game_server_client& client = clientsTable._client_buff[clientIndex];
		if (client.type == game_server_client::TYPE::NONE)
		{
			client.type = game_server_client::TYPE::UNKNOWN;
			client.connected_at_ms = server.uptime_ms;
			client.connection_info = {
				.last_known_address = connection_info.address,
				.last_known_port = connection_info.port,
				.connection_handle = (ui64)connection_info.platform_handle,
			};
			client.type = game_server_client::TYPE::UNKNOWN;

			client.handle._table_index = clientIndex;
			client.handle._fudge = (ui16)server.uptime_ms;

			server.logf(CLIENTS_COMPONENT_NAME, LOG_TYPE::LOG_SUCCESS, 
				"Registered new client connection from platform connection handle %d. Index = %hu, Handle = %d.",
				connection_info.platform_handle, clientIndex, client.handle.value);

			clientsTable._client_count++;

			return &client;
		}
	}

	return nullptr; // Failed to find room. Let the caller handle the consequences.
}

void clients_table_on_connection_lost(game_server& server, game_server_client::client_handle client_handle)
{
	ASSERT(server.client_table != nullptr);
	game_server_clients_table& clientsTable = *server.client_table;	
	
	server.logf(CLIENTS_COMPONENT_NAME, LOG_TYPE::LOG_WARNING,
		"Lost connection with client %d. Removing from active client connections...", client_handle.value);

	// Free client after calling relevant event handler.

	game_server_client& client = clientsTable._client_buff[client_handle._table_index];
	if (client.handle.value != client_handle.value) return; // Stale handle.

	for (ui8 i = 0; i < clientsTable.on_client_disconnected_handler_count; i++)
	{
		clientsTable.on_client_disconnected_handlers[i](server, client);
	}

	// Completely reset client data.
	client = {};
	client.connection_info.connection_handle = game_server_platform::INVALID_NET_CONNECTION_HANDLE;

	clientsTable._client_count--;
}

void clients_table_register_event_handler_client_connection_lost(game_server_clients_table& table, on_client_disconnected_fn handler)
{
	table.on_client_disconnected_handlers[table.on_client_disconnected_handler_count++] = handler;
}

const game_server_client* game_server_get_client_data(game_server& server, game_server_client::client_handle handle)
{
	ASSERT(server.client_table != nullptr);
	ASSERT(handle._table_index < server.client_table->_client_capacity);

	game_server_clients_table& clientsTable = *server.client_table;	
	
	game_server_client& client = clientsTable._client_buff[handle._table_index];
	if (client.type != game_server_client::TYPE::NONE 
		&& client.handle.value == handle.value) return &client;

	return nullptr; // Handle was stale or invalid.
}

void game_server_promote_game_client(game_server& server, game_server_client::client_handle handle, 
	client_send_game_msg_fn send_func,
	client_receive_game_msg_fn receive_func)
{
	ASSERT(server.client_table != nullptr);
	ASSERT(handle._table_index < server.client_table->_client_capacity);
	ASSERT(send_func != nullptr && receive_func != nullptr)

	game_server_clients_table& clientsTable = *server.client_table;	
	game_server_client& client = clientsTable._client_buff[handle._table_index];

	ASSERT(client.type != game_server_client::TYPE::GAME_CLIENT);

	// Change client type and assign send & receive functions.
	client.type = game_server_client::TYPE::GAME_CLIENT;
	client.game_client.send_game_message_func = send_func;
	client.game_client.receive_game_message_func = receive_func;
}

bool game_server_client_send_net_bytes(game_server& server, game_server_client::client_handle handle, const ui8* msg, ui64 msg_size)
{
	ASSERT(server.client_table != nullptr);
	ASSERT(handle._table_index < server.client_table->_client_capacity);
	ASSERT(msg != nullptr && msg_size > 0);

	game_server_clients_table& clientsTable = *server.client_table;	

	const game_server_client* client = game_server_get_client_data(server, handle);
	if (client == nullptr) return false;

	return server.platform->net_send_bytes((game_server_platform::net_connection_handle)client->connection_info.connection_handle,
		msg, msg_size);
}

ui32 game_server_client_receive_net_bytes(game_server& server, game_server_client::client_handle handle, ui8* buff, ui64 buff_size)
{
	ASSERT(server.client_table != nullptr);
	ASSERT(handle._table_index < server.client_table->_client_capacity);
	ASSERT(buff != nullptr && buff_size > 0);

	game_server_clients_table& clientsTable = *server.client_table;	

	const game_server_client* client = game_server_get_client_data(server, handle);
	if (client == nullptr) return 0;

	return server.platform->net_receive_bytes(client->connection_info.connection_handle, buff, buff_size);
}

void game_server_client_drop(game_server& server, game_server_client::client_handle handle)
{
	ASSERT(server.client_table != nullptr);
	ASSERT(handle._table_index < server.client_table->_client_capacity);

	game_server_clients_table& clientsTable = *server.client_table;	

	const game_server_client* client = game_server_get_client_data(server, handle);
	if (client == nullptr) return;

	if (client->connection_info.connection_handle != game_server_platform::INVALID_NET_CONNECTION_HANDLE)
	{
		server.platform->net_close_connection((game_server_platform::net_connection_handle)client->connection_info.connection_handle);
	}

	clients_table_on_connection_lost(server, client->handle);
}

void game_server_client_for_each_of_type(game_server& server, game_server_client::TYPE type, void(*for_each_func)(game_server&, game_server_client&))
{
	ASSERT(server.client_table != nullptr);
	ASSERT(for_each_func != nullptr);

	game_server_clients_table& clientsTable = *server.client_table;	

	for (ui16 clientIndex = 0; clientIndex < clientsTable._client_capacity; clientIndex++)
	{
		game_server_client& client = clientsTable._client_buff[clientIndex];
		if (client.type != game_server_client::TYPE::NONE 
			&& client.type == type)
		{
			for_each_func(server, client);
		}
	}
}

/// END CLIENT TABLE SYSTEM IMPLEMENTATION
