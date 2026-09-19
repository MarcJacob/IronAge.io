// Implementation file for the net communications component of the Win32 Game Server platform.

#include <WinSock2.h>
#include <ws2tcpip.h>

#include "win32_game_server_platform.h"

// Define win32 net component data.
struct win32_net_component
{

};

static win32_net_component WIN32_NET;

void win32_start_net()
{
	win32_log_stdout("Starting Win32 net component...");
}

void win32_stop_net()
{
	win32_log_stdout("Shutting down Win32 net component...");
}

ui16 win32_net_query_new_connections(game_server_platform::in_connection* new_connections, ui16 buff_size)
{
	return 0;
}

ui16 win32_net_query_closed_connections(win32_connection_handle* closed_connections, ui16 buff_size)
{
	return 0;
}

bool win32_net_send_bytes(win32_connection_handle connection, const ui8* bytes, ui32 byte_count)
{
	return false;
}

ui32 win32_net_receive_bytes(win32_connection_handle connection, ui8* buffer, ui32 buff_size)
{
	return 0;
}

void win32_net_close_connection(win32_connection_handle connection)
{
}
