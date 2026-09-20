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
	ui32 address;
	ui16 port;
	SOCKET socket;
};

// Starts the net component which takes care of setting up the app as a server with a listening thread as well as the ability to receive and send data.
// Creates background threads, non-blocking beyond setup.
void win32_net_start();
// Stops the net component. Blocks until all related threads have stopped.
void win32_net_stop();

// Updates the state of active connections.
void win32_net_update_connections();

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

// Simple thread-safe single-consumer single-producer ring-buffer (meaning it's NOT safe to have multiple threads read from or write into the same buffer).
// TODO(Marc): Add a multi-consumer / multi-producer ring-buffer, in case this would make sense for higher net or file system throughput.
template<typename val_type>
struct win32_single_ring_buffer
{
	val_type* _mem;
	ui64 capacity; // Maximum number of values this buffer can hold.

	alignas(64) ui64 _write_cursor;
	alignas(64) ui64 _read_cursor;

	alignas(64) volatile ui64 item_count; // Number of items available for reading.

	// Performs a complete clear of the ring buffer's memory and state, 
	// leaving it in a pristine, empty state a little like my heart after learning programming.
	// Obviously should not be called if there's any change at all that another thread is writing to / reading from it.
	inline void clear()
	{
		ia_memzero(_mem, capacity);
		_write_cursor = 0;
		_read_cursor = 0;
		item_count = 0;
	}

	// Returns the amount of items that can still be written into the buffer before it's full.
	inline ui64 get_free_capacity()
	{
		return capacity - item_count;
	}

	// Reads a single item from the buffer. Returns whether the read was successful.
	inline bool read(val_type& out_val)
	{
		if (item_count == 0) return false;

		out_val = *(_mem + _read_cursor);
		_read_cursor = (_read_cursor + 1) % capacity;

		InterlockedDecrementRelease(&item_count);

		return true;
	}
	
	// Copies up to count items from the ring buffer without consuming them.
	// Assumes the buffer is large enough to hold the requested item count contiguously.
	// Returns number of items copied.
	inline ui64 peek(val_type* read_buff, ui64 count)
	{
		ui64 readCount = item_count > count ? count : item_count;
		if (readCount == 0) return 0;

		if (_read_cursor + readCount > capacity)
		{
			// Wrap around read.
			ui64 read_1_count = capacity - _read_cursor;
			ui64 read_2_count = readCount - read_1_count;

			ia_memcpy(read_buff, _mem + _read_cursor, read_1_count * sizeof(val_type));
			ia_memcpy(read_buff + read_1_count, _mem, read_2_count * sizeof(val_type));
		}
		else
		{
			// Straight read.
			ia_memcpy(read_buff, _mem + _read_cursor, readCount * sizeof(val_type));
		}

		return readCount;
	}

	// Consumes up to count items without copying them. Returns number of items discarded.
	inline ui64 discard(ui64 count)
	{
		ui64 discardCount = item_count > count ? count : item_count;
		if (discardCount == 0) return 0;

		_read_cursor = (_read_cursor + discardCount) % capacity;
		InterlockedExchangeSubtract(&item_count, discardCount);

		return discardCount;
	}

	// Reads up to count items from the ring buffer in one go.
	// Assumes the buffer is large enough to hold the requested item count contiguously.
	// Returns number of items read.
	inline ui64 read(val_type* read_buff, ui64 count)
	{
		ui64 readCount = peek(read_buff, count);
		discard(readCount);
		return readCount;
	}

	// Writes a single item into the buffer. Returns whether the write was successful.
	inline bool write(const val_type& in_val)
	{
		if (item_count == capacity) return false;

		*(_mem + _write_cursor) = in_val;
		_write_cursor = (_write_cursor + 1) % capacity;

		InterlockedIncrementRelease(&item_count);

		return true;
	}

	// Writes up to count items to the ring buffer in one go.
	// Assumes the buffer holds the specified item count contiguously.
	// Returns number of items written.
	inline ui64 write(const val_type* write_buff, ui64 count)
	{
		ui64 writeCount = get_free_capacity() > count ? count : get_free_capacity();
		if (writeCount == 0) return 0;

		if (_write_cursor + writeCount > capacity)
		{
			// Wrap around write.
			ui64 write_1_count = capacity - _write_cursor;
			ui64 write_2_count = writeCount - write_1_count;

			ia_memcpy(_mem + _write_cursor, write_buff, write_1_count * sizeof(val_type));
			ia_memcpy(_mem, write_buff + write_1_count, write_2_count * sizeof(val_type));
		}
		else
		{
			// Straight write.
			ia_memcpy(_mem + _write_cursor, write_buff, writeCount * sizeof(val_type));
		}

		_write_cursor = (_write_cursor + writeCount) % capacity;
		InterlockedExchangeAdd(&item_count, writeCount);

		return writeCount;
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
