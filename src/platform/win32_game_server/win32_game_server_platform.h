// Shared symbols & includes between the various components of the Win32 game server platform code.

#ifndef WIN32_GAME_SERVER_PLATFORM_INCLUDED
#define WIN32_GAME_SERVER_PLATFORM_INCLUDED

#include "core.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Control

void win32_shutdown(int code);

// Logging

// Logs message to stdout.
void win32_log_stdout(const char* msg);
// Logs formatted message to stdout.
void win32_logf_stdout(const char* msg, ...);
// Logs message to stderr.
void win32_log_stderr(const char* msg);
// Logs formatted message to stderr.
void win32_logf_stderr(const char* msg, ...);

// Reads in an entire file, or gets its size of read_buff is null.
ui64 win32_read_file(const char* filename, ui8* read_buff, ui64 buff_size);
// Write a file, overwriting whatever was there if anything.
bool win32_write_file(const char* filename, const ui8* data, ui64 size);

// Networking

typedef game_server_platform::net_connection_handle win32_connection_handle;

struct win32_in_connection
{
	game_server_platform::in_connection in_connection_data;

	SOCKET socket;
};

// Starts the net component which takes care of setting up the app as a server with a listening thread as well as the ability to receive and send data.
// Creates background threads, non-blocking beyond setup.
void win32_start_net();
// Stops the net component. Blocks until all related threads have stopped.
void win32_stop_net();

// Returns all new net connections since the last time this was called.
ui16 win32_net_query_new_connections(game_server_platform::in_connection* new_connections, ui16 buff_size);

// Returns all closed net connections since the last time this was called.
ui16 win32_net_query_closed_connections(win32_connection_handle* closed_connections, ui16 buff_size);

// Attempts to send the bytes on the connection related to the handle. Returns success.
bool win32_net_send_bytes(win32_connection_handle connection, const ui8* bytes, ui32 byte_count);

// Reads any data that may be waiting on the connection into the buffer, or how many bytes are waitig if buffer == null.
ui32 win32_net_receive_bytes(win32_connection_handle connection, ui8* buffer, ui32 buff_size);

// Forces a connection to be closed, dropping any data that may have still been waiting for reception.
void win32_net_close_connection(win32_connection_handle connection);

// Simple thread-safe single-consumer single-producer ring-buffer.
// Reading from the buffer consumes the item and returns a copy.
template<typename val_type>
struct win32_single_ring_buffer
{
	val_type* _mem;
	ui32 capacity; // Maximum number of values this buffer can hold.

	alignas(64) ui32 _write_cursor;
	alignas(64) ui32 _read_cursor;

	alignas(64) volatile ui32 _item_count; // Number of items available for reading.

	bool read(val_type& out_val)
	{
		if (_item_count == 0) return false;

		out_val = *(_mem + _read_cursor);
		_read_cursor = (_read_cursor + 1) % capacity;

		InterlockedDecrementRelease(&_item_count);

		return true;
	}

	bool write(const val_type& in_val)
	{
		if (_item_count == capacity) return false;

		*(_mem + _write_cursor) = in_val;
		_write_cursor = (_write_cursor + 1) % capacity;

		InterlockedIncrementRelease(&_item_count);

		return true;
	}
};

// Creates a new single ring buffer for the given item type, with the given capacity (expressed as a number of items).
// In case of failure, the returned buffer has no assigned memory or capacity.
template<typename val_type>
win32_single_ring_buffer<val_type> win32_create_single_ring_buffer(mem_arena& mem, ui32 capacity)
{
	win32_single_ring_buffer<val_type> buff = {};
	buff._mem = mem.alloc<val_type>(capacity);
	if (buff._mem == nullptr) return buff;

	buff.capacity = capacity;
	return buff;
}

#endif // WIN32_GAME_SERVER_PLATFORM_INCLUDED
