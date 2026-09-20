// Core include for the Game Server Main Lifecycle code.
// Contains essential game server symbols and lifecycle functions for use by the platform to start and maintain a server app.

#include "core.h"

#ifdef GAME_SERVER_MAIN_INCLUDED
	static_assert(0, "Game server main already included... do you have two platforms linking at the same time ?");
#else
#define GAME_SERVER_MAIN_INCLUDED
#endif

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

	typedef void (*shutdown_func)(int code);
	// Platform function: Requests standard shutdown when the game server has stopped.
	shutdown_func shutdown;

	// PLATFORM LOGGING

	typedef void (*log_stdout_func)(const char*);
	// Platform function: Takes in a null-terminated string and transfers it to standard output.
	log_stdout_func log_stdout;

	typedef void (*logf_stdout_func)(const char*, ...);
	// Platform function: Takes in a null-terminated format string and format parameters and transfers it to standard output.
	logf_stdout_func logf_stdout;

	typedef void (*log_stderr_func)(const char*);
	// Platform function: Takes in a null-terminated string and transfers it to error output.
	log_stderr_func log_stderr;

	typedef void(*logf_stderr_func)(const char*, ...);
	// Platform function: Takes in a null-terminated format string and format parameters and transfers it to error output.
	logf_stderr_func logf_stderr;

	// PLATFORM NET

	typedef ui32 net_connection_handle;	// Unique identifier for an active connection. Note that some platforms may re-use the same handle, 
										// meaning that the server should always check for closed connections first to ensure the handle
										// is available on its end. Connections should stay alive on the platform at least so long as data is waiting to be read.

	// Structured information about a new inbound connection.
	struct in_connection
	{
		net_connection_handle platform_handle;
		ui32 address; // Used to recognize the same peer over multiple connections.
		ui16 port; // Platform port number used by this connection.
	};

	typedef ui16 (*net_query_new_connections_func)(in_connection* new_connections_buff, ui16 buff_size);
	// Platform function: Takes in a target connections buffer and max size and fills in any new incoming connections.
	// Returns the number of new connections. If the buffer size is reached, call the function again to get the rest.
	// Thread safety is not guaranteed beyond a single querying thread.
	net_query_new_connections_func net_query_new_connections;

	typedef ui16 (*net_query_closed_connections_func)(net_connection_handle* closed_handles_buff, ui16 buff_size);
	// Platform function: Takes in a target handles buffer and max size and fills in any closed connections.
	// Returns the number of closed connections. If the buffer size is reached, call the function again to get the rest.
	// Thread safety is not guaranteed beyond a single querying thread.
	net_query_closed_connections_func net_query_closed_connections;

	typedef bool (*net_send_bytes_func)(net_connection_handle handle, const ui8* bytes, ui32 bytes_count);
	// Platform function: buffers bytes for sending towards an existing connection identified by a handle.
	// Returns whether the data was successfully buffered / sent on the platform. Failure usually means the connection was closed,
	// or that there's too much data already buffered for sending.
	// Thread safety is not guaranteed on the same connection beyond a single sending thread.
	net_send_bytes_func net_send_bytes;

	typedef ui32 (*net_receive_bytes_func)(net_connection_handle handle, ui8* buff, ui32 buff_size);
	// Platform function: receives bytes from a connection based on its handle. If max buffer size is reached,
	// call the function again to get the rest. If buff is null, returns the number of bytes waiting for reception.
	// Returns the number of bytes received, with 0 meaning that no data has been received (NOT that the connection has closed).
	// Thread safety is not guaranteed on the same connection beyond a single reception thread.
	net_receive_bytes_func net_receive_bytes;

	typedef void (*net_close_connection_func)(net_connection_handle handle);
	// Platform function: requests the platform close the connection related to the handle, if any.
	// Forces the connection to be dropped from the platform, and erases any data that may have been waiting to be read.
	// Thread safety is not guaranteed beyond a single requesting thread.
	net_close_connection_func net_close_connection;

	// PLATFORM FILES

	typedef ui64 (*read_file_func)(const char* filename, ui8* read_buff, ui64 buff_size);
	// Platform function: Synchronously reads / loads in an entire file's contents into the target buffer, if it is large enough.
	// If read_buff is null, performs a "dry run" and returns the file size.
	// Returns the number of bytes read, 0 if the file does not exist / is inaccessible, or the buffer is too small.
	// The file is located in the "server resources storage", whatever that means for the host platform.
	read_file_func read_resource_file;

	typedef bool (*write_file_func)(const char* filename, const ui8* data, ui64 size);
	// Platform function: Synchronously writes the buffer to a file with the given name, creating it or overwriting it.
	// Returns whether the write was successful.
	// The file is located in the "server resources storage", whatever that means for the host platform.
	write_file_func write_resource_file;
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


