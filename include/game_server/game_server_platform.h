// Core include for the Game Server Main Lifecycle code.
// Contains essential game server symbols and lifecycle functions for use by the platform to start and maintain a server app.

#include "core.h"

#ifndef GAME_SERVER_PLATFORM_INCLUDED
#define GAME_SERVER_PLATFORM_INCLUDED

// Platform capabilities handed to the server code so it can, non-exhaustively:
// - Log messages
// - Read & Write files
// - Establish network connections
// - Create threads
// ...
// The platform for the game server specifically does not allow memory allocations. The memory the server has to work with is given on initialization.
struct game_server_platform
{
	// PLATFORM CONTROL

	typedef void (*shutdown_fn)(game_server_platform& platform, int code);
	// Platform function: Requests standard shutdown when the game server has stopped.
	shutdown_fn shutdown_func;
	void shutdown(int code) {
		shutdown_func(*this, code);
	}

	// PLATFORM LOGGING

	typedef void (*log_fn)(game_server_platform& platform, LOG_TYPE type, const char*);
	// Platform function: Takes in a log type and a null-terminated string and transfers it to the platform's log output for that type.
	log_fn log_func;
	void log(LOG_TYPE type, const char* msg) {
		log_func(*this, type, msg);
	}
	inline void log(const char* msg) { log(LOG_NORMAL, msg); }

	typedef void (*logf_fn)(game_server_platform& platform, LOG_TYPE type, const char*, ...);
	// Platform function: Takes in a log type, a null-terminated format string and format parameters and transfers it to the platform's log output for that type.
	logf_fn logf_func;
	template<typename... args_types>
	void logf(LOG_TYPE type, const char* format, args_types... args) {
		logf_func(*this, type, format, args...);
	}
	template<typename... args_types>
	inline void logf(const char* format, args_types... args) { logf(LOG_NORMAL, format, args...); }

	// PLATFORM NET

	typedef ui32 net_connection_handle;	// Unique identifier for an active connection. Note that some platforms may re-use the same handle, 
										// meaning that the server should always check for closed connections first to ensure the handle
										// is available on its end. Connections should stay alive on the platform at least so long as data is waiting to be read.
	static constexpr net_connection_handle INVALID_NET_CONNECTION_HANDLE = ~0;

	// Structured information about a new inbound connection.
	struct in_connection
	{
		net_connection_handle platform_handle;
		ui32 address; // Used to recognize the same peer over multiple connections.
		ui16 port; // Platform port number used by this connection.
	};

	typedef ui16 (*net_query_new_connections_fn)(game_server_platform& platform, in_connection* new_connections_buff, ui16 buff_size);
	// Platform function: Takes in a target connections buffer and max size and fills in any new incoming connections.
	// Returns the number of new connections. If the buffer size is reached, call the function again to get the rest.
	// Thread safety is not guaranteed beyond a single querying thread.
	net_query_new_connections_fn net_query_new_connections_func;
	ui16 net_query_new_connections(in_connection* new_connections_buff, ui16 buff_size) {
		return net_query_new_connections_func(*this, new_connections_buff, buff_size);
	}

	typedef ui16 (*net_query_closed_connections_fn)(game_server_platform& platform, net_connection_handle* closed_handles_buff, ui16 buff_size);
	// Platform function: Takes in a target handles buffer and max size and fills in any closed connections.
	// Returns the number of closed connections. If the buffer size is reached, call the function again to get the rest.
	// Thread safety is not guaranteed beyond a single querying thread.
	net_query_closed_connections_fn net_query_closed_connections_func;
	ui16 net_query_closed_connections(net_connection_handle* closed_handles_buff, ui16 buff_size) {
		return net_query_closed_connections_func(*this, closed_handles_buff, buff_size);
	}

	typedef bool (*net_send_bytes_fn)(game_server_platform& platform, net_connection_handle handle, const ui8* bytes, ui32 bytes_count);
	// Platform function: buffers bytes for sending towards an existing connection identified by a handle.
	// Returns whether the data was successfully buffered / sent on the platform. Failure usually means the connection was closed,
	// or that there's too much data already buffered for sending.
	// Thread safety is not guaranteed on the same connection beyond a single sending thread.
	net_send_bytes_fn net_send_bytes_func;
	bool net_send_bytes(net_connection_handle handle, const ui8* bytes, ui32 bytes_count) {
		return net_send_bytes_func(*this, handle, bytes, bytes_count);
	}

	typedef ui32 (*net_receive_bytes_fn)(game_server_platform& platform, net_connection_handle handle, ui8* buff, ui32 buff_size);
	// Platform function: receives bytes from a connection based on its handle. If max buffer size is reached,
	// call the function again to get the rest. If buff is null, returns the number of bytes waiting for reception.
	// Returns the number of bytes received, with 0 meaning that no data has been received (NOT that the connection has closed).
	// Thread safety is not guaranteed on the same connection beyond a single reception thread.
	net_receive_bytes_fn net_receive_bytes_func;
	ui32 net_receive_bytes(net_connection_handle handle, ui8* buff, ui32 buff_size) {
		return net_receive_bytes_func(*this, handle, buff, buff_size);
	}

	typedef void (*net_close_connection_fn)(game_server_platform& platform, net_connection_handle handle);
	// Platform function: requests the platform close the connection related to the handle, if any.
	// Forces the connection to be dropped from the platform, and erases any data that may have been waiting to be read.
	// Data already queued with net_send_bytes is flushed first (dropped if the peer can't take it).
	// Thread safety is not guaranteed beyond a single requesting thread.
	net_close_connection_fn net_close_connection_func;
	void net_close_connection(net_connection_handle handle) {
		net_close_connection_func(*this, handle);
	}

	// PLATFORM FILES

	typedef ui64 (*read_file_fn)(game_server_platform& platform, const char* filename, ui8* read_buff, ui64 buff_size);
	// Platform function: Synchronously reads / loads in an entire file's contents into the target buffer, if it is large enough.
	// If read_buff is null, performs a "dry run" and returns the file size.
	// Returns the number of bytes read, 0 if the file does not exist / is inaccessible, or the buffer is too small.
	// The file is located in the "server resources storage", whatever that means for the host platform.
	read_file_fn read_resource_file_func;
	ui64 read_resource_file(const char* filename, ui8* read_buff, ui64 buff_size) {
		return read_resource_file_func(*this, filename, read_buff, buff_size);
	}

	typedef bool (*write_file_fn)(game_server_platform& platform, const char* filename, const ui8* data, ui64 size);
	// Platform function: Synchronously writes the buffer to a file with the given name, creating it or overwriting it.
	// Returns whether the write was successful.
	// The file is located in the "server resources storage", whatever that means for the host platform.
	write_file_fn write_resource_file_func;
	bool write_resource_file(const char* filename, const ui8* data, ui64 size) {
		return write_resource_file_func(*this, filename, data, size);
	}
};


struct game_server;
struct game_server_init_params;

/**
 * Initializes a new game server from the provided platform functions, giving it its memory footprint.
 * If successful, returns pointer to the initialized server structure. 
 * Update over time using game_server_tick.
 */
game_server* game_server_init(game_server_platform& platform, game_server_init_params& init_params, ui8* memory, ui64 memory_size);

/**
 * Integrates the passage of time into the game server simulation, triggering the ticking of ongoing matches as needed.
 * The time parameter should be the total platform uptime since the server was started.
 */
void game_server_tick(game_server& server, time_ms time_ms);

/**
 * Called on platform shutdown, no matter the reason.
 * Used to allow the server time to perform cleanup operations before the platform shuts down its own functionality.
 */
void game_server_stop(game_server& server);

#endif // GAME_SERVER_PLATFORM_INCLUDED
