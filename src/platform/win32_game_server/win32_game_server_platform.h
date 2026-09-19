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

#endif // WIN32_GAME_SERVER_PLATFORM_INCLUDED
