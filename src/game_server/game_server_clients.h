// Symbol declarations related to core game server client management & querying.

#ifndef GAME_SERVER_CLIENTS_INCLUDED
#define GAME_SERVER_CLIENTS_INCLUDED

struct game_message_header;
struct game_server_client;

// Defines a function able to handle sending a game message towards a game server client. The client must be of type GAME_CLIENT.
using client_send_game_msg_fn = bool(*)(game_server_client& client, const game_message_header& message);

// Defines a function able to peek at the next received game message from a game server client, if any. The client must be of type GAME_CLIENT.
// Returns whether a message is available, in which case out_message_ptr will point to the actual data in memory making up the message.
// It can be read as is or copied somewhere else, and stays valid, and the same, until the message is consumed.
using client_peek_game_msg_fn = bool(*)(game_server_client& client, game_message_header*& out_message_ptr);

// Defines a function able to consume the game message currently at the front of a game server client's received messages, so that the next peek
// gives the following one. Does nothing if there is no message. The client must be of type GAME_CLIENT.
using client_consume_game_msg_fn = void(*)(game_server_client& client);

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
			ui16 _fudge; // Meaningless number, only there to tell apart a handle created for a previous client on the same index from the client currently at that index.
			ui16 _table_index; // Index into the clients table storage.
		};
		ui32 value;
	} handle;

	time_ms connected_at_ms; // Server uptime at which this client's connection was registered.

	struct
	{
		ui32 last_known_address;	// Last address this client used to connect. Established on first connection, can change later if client is recognized on another connection.
		ui16 last_known_port;		// Last port this client used to connect. Established & changed like address, and can identify which connection type it should be.
		ui64 connection_handle;		// Stored platform connection handle.
	} connection_info;

	// Supported types of clients. Indexes into the event handler tables to allow other sub-systems to react to client events.
	enum class TYPE
	{
		NONE,			 // Structure does not refer to an actual client but can be used to house a new client for a new connection.
		UNKNOWN,		 // Client has established a connection but hasn't authentified themselves as any type of client supported by the server.
		NON_GAME_CLIENT, // Client is connected and has been taken in by one of the server sub-components pending a possible upgrade a full Game Client.
		GAME_CLIENT,	 // Client has established a full two-way connection allowing real-time game synchronization traffic.
		ADMIN,			 // Client is authentified as an administrator and can send commands & special queries to the server.
		TYPE_COUNT,
	} type;

	void* connection_context;	// Extra contextual data related to the specific server component in charge of this connection (Web Server, Native Server...).
								// Allows server components to recognize this client's connection as being managed by them, and find their associated, specific data.

	// Discriminated union of data associated to each connection type.
	union 
	{
		struct
		{
			// Track total received data. We don't store anything for unknown client and we may choose to kick them out after a point, or even blacklist their address.

			ui64 traffic_size; // Total amount of bytes received from this client.
		} unknown;

		struct
		{
			ui8 _empty;
		} non_game_client;

		struct
		{
			// Assigned by server sub-component in charge of actual client connection.
			client_send_game_msg_fn send_game_message_func;
			// Assigned by server sub-component in charge of actual client connection.
			client_peek_game_msg_fn peek_game_message_func;
			// Assigned by server sub-component in charge of actual client connection.
			client_consume_game_msg_fn consume_game_message_func;
		} game_client;
	};

	// Sends message to this game client. Client must be of type GAME_CLIENT.
	// Returns whether the message was successfully sent.
	inline bool game_client_send_message(const game_message_header& msg) 
	{ 
		ASSERT(type == TYPE::GAME_CLIENT);
		ASSERT(game_client.send_game_message_func != nullptr);

		return game_client.send_game_message_func(*this, msg);
	}

	// Peeks at the next message received from this game client. Client must be of type GAME_CLIENT.
	// Returns whether a message is available, in which case out_msg_ptr will point to it. The message stays the same until consumed.
	inline bool game_client_peek_message(game_message_header*& out_msg_ptr)
	{
		ASSERT(type == TYPE::GAME_CLIENT);
		ASSERT(game_client.peek_game_message_func != nullptr);

		return game_client.peek_game_message_func(*this, out_msg_ptr);
	}

	// Consumes the message last peeked, so the next peek returns the next message. Client must be of type GAME_CLIENT.
	// Every peeked message MUST be consumed once done with.
	inline void game_client_consume_message()
	{
		ASSERT(type == TYPE::GAME_CLIENT);
		ASSERT(game_client.consume_game_message_func != nullptr);

		game_client.consume_game_message_func(*this);
	}
};

struct game_server_clients_table;

using on_client_disconnected_fn = void(*)(game_server& server, game_server_client& disconnected_client);

// Main lifecycle / control functions.

// Initializes the clients table associated with the server.
// max_client_count specifies the maximum amount of concurrent client connections supported by the server.
void clients_table_init(game_server& server, ui16 max_client_count);

// Registers a new connection with the clients table. The new client starts out as type UNKNOWN.
// If successful, returns a pointer to the client structure now associated with this connection.
game_server_client* clients_table_register_new_connection(game_server& server, game_server_platform::in_connection& connection_info);

// Signals the table that a connection was lost or dropped.
void clients_table_on_connection_lost(game_server& server, game_server_client::client_handle handle);

void clients_table_register_event_handler_client_connection_lost(game_server_clients_table& table, game_server_client::TYPE client_type, on_client_disconnected_fn handler);

// Extensions to server functionality

// Retrieves pointer to game server client data associated with the handle.
const game_server_client* game_server_get_client_data(game_server& server, game_server_client::client_handle handle);

// Promotes a client connection to GAME_CLIENT status, allowing it to take part in the game server / game messages messaging protocols.
// Used by server sub-components. send_func, peek_func and consume_func must contain valid functions for the client to use to route messages in and out.
void game_server_promote_game_client(game_server& server, game_server_client::client_handle handle,
	client_send_game_msg_fn send_func,
	client_peek_game_msg_fn peek_func,
	client_consume_game_msg_fn consume_func);

// Sends bytes along a client's associated platform network connection. To be used by server subcomponents.
// Game Server / Game Client logic should use the Game Client equivalent.
bool game_server_client_send_net_bytes(game_server& server, game_server_client::client_handle handle, const ui8* msg, ui64 msg_size);

// Receive bytes along a client's associated platform network connection. To be used by server subcomponents.
// Game Server / Game Client logic should use the Game Client equivalent.
ui32 game_server_client_receive_net_bytes(game_server& server, game_server_client::client_handle handle, ui8* buff, ui64 buff_size);

// Unilaterally drops the platform connection associated with the client, and the client itself from the table.
void game_server_client_drop(game_server& server, game_server_client::client_handle handle);

// Executes a function over every client of the given type.
void game_server_client_for_each_of_type(game_server& server, game_server_client::TYPE type, void(*for_each_func)(game_server&, game_server_client& client));

#endif // GAME_SERVER_CLIENTS_INCLUDED
