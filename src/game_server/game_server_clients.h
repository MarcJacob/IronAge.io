// Symbol declarations related to core game server client management & querying.

#ifndef GAME_SERVER_CLIENTS_INCLUDED
#define GAME_SERVER_CLIENTS_INCLUDED

#include "core.h"

#include "game_server/game_server_platform.h"

#include "game_common/game_messages.h"

struct game_server;
struct game_server_platform;
struct game_server_client;

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

        // 
		struct
		{
			ui8 _empty;
		} non_game_client;

        // Holds client state when it has been promoted to a Game Client, able to send and receive game messages.
		struct
		{
            // @TODO(Marc): Turn those into special Queue arenas that can be made thread-safe when connector components (IE Web Server) are made to run on a different thread.
            mem_arena* in_messages_buffer; // Buffer inside which incoming game messages are put. Contains Game Messages back to back in reception order.
            mem_arena* out_messages_buffer; // Buffer inside which outgoing game messages are put. Contains Game Messages back to back in emission order.

		    ui16 match_slot; // The match slot this client has joined, if any (INVALID_MATCH_SLOT if none).
            match_player_id player_index; // If in an ongoing match, index of the player controlled by this client.
		} game_client;
	};

	// Sends message to this game client. Client must be of type GAME_CLIENT.
	// Returns whether the message was successfully sent.
	inline bool game_client_send_message(const game_message_header& msg)
	{ 
		ASSERT(type == TYPE::GAME_CLIENT);
        ASSERT(game_client.out_messages_buffer != nullptr && game_client.out_messages_buffer->mem_size > 0);
        
        void* bufferTarget = game_client.out_messages_buffer->alloc(sizeof(game_message_header) + msg.payload_size, 1); // Alignment 1: messages sit back to back, no padding.
        if (bufferTarget == nullptr) return false;

        ia_memcpy(bufferTarget, &msg, sizeof(game_message_header) + msg.payload_size);
        return true;
	}

    // Queries the game client for a pointer to its internal message reception buffer into which its connector component
    // pushes game messages.
	inline const void* game_client_get_messages_reception_buffer(ui64& out_received_bytes)
	{
		ASSERT(type == TYPE::GAME_CLIENT);
        ASSERT(game_client.in_messages_buffer != nullptr && game_client.in_messages_buffer->mem_size > 0);

        out_received_bytes = game_client.in_messages_buffer->allocated_count;
        return game_client.in_messages_buffer->mem_start;
	}

    // Empties out the game client's internal message reception buffer.
    inline void game_client_clear_reception_buffer()
    {
        ASSERT(type == TYPE::GAME_CLIENT);
        game_client.in_messages_buffer->clear();
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

// Extensions to server functionality

// Retrieves pointer to game server client data associated with the handle.
game_server_client* game_server_get_client_data(game_server& server, game_server_client::client_handle handle);

// Promotes a client connection to GAME_CLIENT status, allowing it to take part in the game server / game messages messaging protocols.
// Whoever requests the promotion is in charge of providing appropriate reception & send buffers from which the server code and send & receive game messages.
void game_server_promote_game_client(game_server& server, game_server_client::client_handle handle, mem_arena* reception_buffer_ptr, mem_arena* send_buffer_ptr);

// Sends bytes along a client's associated platform network connection. To be used by server subcomponents.
// Game Server / Game Client logic should use the Game Client equivalent.
bool game_server_client_send_net_bytes(game_server& server, game_server_client::client_handle handle, const ui8* msg, ui64 msg_size);

// Receive bytes along a client's associated platform network connection. To be used by server subcomponents.
// Game Server / Game Client logic should use the Game Client equivalent.
ui32 game_server_client_receive_net_bytes(game_server& server, game_server_client::client_handle handle, ui8* buff, ui64 buff_size);

// Unilaterally drops the platform connection associated with the client.
// The client stays in the table until its connection is entirely closed.
void game_server_client_drop(game_server& server, game_server_client::client_handle handle);

// Executes a function over every client of the given type.
void game_server_client_for_each_of_type(game_server& server, game_server_client::TYPE type, void(*for_each_func)(game_server&, game_server_client& client));

#endif // GAME_SERVER_CLIENTS_INCLUDED
