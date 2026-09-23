// Symbol declarations related to core game server client management & querying.

#ifndef GAME_SERVER_CLIENTS_INCLUDED
#define GAME_SERVER_CLIENTS_INCLUDED

// Forward declaration of client type data structures.
struct http_client;

// Core data associated with a client connection on the game server.
// Client connections cover ALL sorts of inward connections started from the outside, and can survive the loss of the platform connection.
// TODO: User info & authentication system.
struct game_server_client
{
	// Identification structure that can be handed to the clients table to get a pointer to a live client.
	// Safer than a pointer, and can recognize handle collision.
	union client_handle
	{
		struct
		{
			ui16 _create_time_ms; // Allows differentiating between a handle created for a previous client on the same index and the client currently at that index.
			ui16 _table_index; // Index into the clients table storage.
		};
		ui32 value;
	} handle;

	enum class STATE
	{
		FREE,				// Indicates this structure does not refer to any actual client connection and can be used for one.
		// ... TODO(Marc): Intermediate states to support re-connection.
		ONLINE,				// Client is online and has an active connection with platform networking.
	} state;

	struct
	{
		ui32 last_known_address;	// Last address this client used to connect. Established on first connection, can change later if client is recognized on another connection.
		ui16 last_known_port;		// Last port this client used to connect. Established & changed like address, and can identify which connection type it should be.
		ui64 connection_handle;		// Stored platform connection handle.
	} connection_info;

	// Supported types of clients. Indexes into the event handler tables to allow other sub-systems to react to client events.
	enum class TYPE
	{
		UNKNOWN,		// Client has established a connection but hasn't authentified themselves as any type of client supported by the server.
		HTTP,			// Client is connected through HTTP. They can request files or upgrade to one of the more advanced client types.
		GAME_CLIENT,	// Client has established a full two-way connection allowing real-time game synchronization traffic.
		ADMIN,			// Client is authentified as an administrator and can send commands & special queries to the server.
		TYPE_COUNT,
	} type;

	// Discriminated union of data associated to each connection type.
	union 
	{
		struct
		{
			// Track total received data. We don't store anything for unknown client and we may choose to kick them out after a point, or even blacklist their address.

			ui64 traffic_size; // Total amount of bytes received from this client.
		} unknown;

		http_client* http;
	};
};

struct game_server_clients_table;

using on_client_disconnected_fn = void(*)(game_server& server, game_server_client& disconnected_client);

// Main lifecycle / control functions.

// Initializes the clients table associated with the server.
// max_client_count specifies the maximum amount of concurrent client connections supported by the server.
void game_server_init_clients_table(game_server& server, ui16 max_client_count);

// Registers a new connection with the clients table, associating it with an existing client or creating a new one for it.
// If successful, returns a pointer to the client structure now associated with this connection.
game_server_client* game_server_clients_register_connection(game_server& server, game_server_platform::in_connection& connection_info);

// Signals the table that a connection was lost so an associated client can be updated.
void game_server_clients_connection_lost(game_server& server, game_server_platform::net_connection_handle connection_handle);

// Added server functionality

game_server_client* game_server_get_client_data(game_server& server, game_server_client::client_handle handle);

#endif // GAME_SERVER_CLIENTS_INCLUDED
