// Implementation file for the net communications component of the Win32 Game Server platform.

#include <WinSock2.h>
#include <ws2tcpip.h>

#include "win32_game_server_platform.h"

// Define win32 net component data.
struct win32_net_component
{
	WSAData wsa_data;

	struct listen_server_data
	{
		SOCKET socket;
		DWORD thread_id;
		HANDLE thread_handle;
		
		// Output buffer of the listen server.
		win32_single_ring_buffer<win32_in_connection> new_connections_buffer;
	} listen_server;

	mem_arena component_memory;

	win32_single_ring_buffer<win32_connection_handle> closed_connections_buffer;
};

static win32_net_component WIN32_NET;

static constexpr ui64 NET_COMPONENT_MEMORY_SIZE = MiB(512);
static constexpr ui16 NEW_CONNECTIONS_BUFFER_CAPACITY = 256;
static constexpr ui16 CLOSED_CONNECTIONS_BUFFER_CAPACITY = 256;

unsigned long listen_server_func(LPVOID context);

void win32_start_net()
{
	win32_log_stdout("Starting Win32 net component...");

	WIN32_NET = {};

	if (WSAStartup(MAKEWORD(2, 2), &WIN32_NET.wsa_data) != 0)
	{
		win32_logf_stderr("Failed to start WSA. Error code = %d", WSAGetLastError());
		return;
	}

	// Allocate component memory.
	ui8* component_mem = (ui8*)VirtualAlloc(NULL, NET_COMPONENT_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WIN32_NET.component_memory = mem_arena_create(component_mem, NET_COMPONENT_MEMORY_SIZE);

	// Create buffers.
	WIN32_NET.listen_server.new_connections_buffer = win32_create_single_ring_buffer<win32_in_connection>(
		WIN32_NET.component_memory,
		NEW_CONNECTIONS_BUFFER_CAPACITY);

	WIN32_NET.closed_connections_buffer = win32_create_single_ring_buffer<win32_connection_handle>(
		WIN32_NET.component_memory,
		CLOSED_CONNECTIONS_BUFFER_CAPACITY);

	// Create listen server thread.
	WIN32_NET.listen_server.thread_handle = CreateThread(NULL, NULL, listen_server_func, nullptr, NULL, &WIN32_NET.listen_server.thread_id);
	if (WIN32_NET.listen_server.thread_handle == NULL)
	{
		win32_logf_stderr("Failed to start listen server thread. Error code = %d", GetLastError());
		WSACleanup();
		return;
	}
}

void win32_stop_net()
{
	win32_log_stdout("Shutting down Win32 net component...");

	// Close listen server socket which will trigger the listen server thread to end.
	closesocket(WIN32_NET.listen_server.socket);

	WaitForSingleObject(WIN32_NET.listen_server.thread_handle, INFINITE);

	WSACleanup();
}

ui16 win32_net_query_new_connections(game_server_platform::in_connection* new_connections, ui16 buff_size)
{
	win32_in_connection newConnection;
	ui16 writeCount = 0;
	while (WIN32_NET.listen_server.new_connections_buffer.read(newConnection))
	{
		new_connections[writeCount] = newConnection.in_connection_data;
		writeCount++;

		if (writeCount == buff_size) break;
	}
	return writeCount;
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

// Runs a listen server on loop, accepting new connections and adding them to the new connections buffer.
unsigned long listen_server_func(LPVOID context)
{
	win32_net_component::listen_server_data& listenServer = WIN32_NET.listen_server;

	listenServer.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenServer.socket == INVALID_SOCKET)
	{
		win32_log_stderr("Failed to create listen server socket.");
		return 1;
	}

	struct sockaddr_in bind_addr = {};
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(8000);
	bind_addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);

	if (bind(listenServer.socket, (sockaddr*)&bind_addr, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		win32_logf_stderr("Failed to bind listen server socket. WSA error = %d", WSAGetLastError());
		return 1;
	}
	
	if (listen(listenServer.socket, 256) == SOCKET_ERROR)
	{
		win32_log_stderr("Failed to start listening on listen server socket.");
		return 1;
	}

	sockaddr_in newConnectionAddr = {};
	i32 addrLen = sizeof(newConnectionAddr);

	for (;;)
	{
		newConnectionAddr = {};
		addrLen = sizeof(newConnectionAddr);
		SOCKET newSock = accept(listenServer.socket, (sockaddr*)&newConnectionAddr, &addrLen);
		if (newSock == INVALID_SOCKET)
		{
			i32 err = WSAGetLastError();
			win32_logf_stderr("Error accepting new connection on listen server. Error code = %d", err);

			// Check if error is fatal.
			if (err == WSAEINTR || err == WSAENOTSOCK)
			{
				// End thread with code 1.
				return 1;
			}
			continue;
		}

		char addrBuff[128];
		inet_ntop(AF_INET, &newConnectionAddr.sin_addr, addrBuff, sizeof(addrBuff));
		win32_logf_stdout("Incoming connection... Socket = %llu.\n\tAddress = %s\n\tPort = %d", newSock, addrBuff, newConnectionAddr.sin_port);

		win32_in_connection newConnection = {
			.in_connection_data = {
				.platform_handle = 0,
				.address = newConnectionAddr.sin_addr.S_un.S_addr
			},
			.socket = newSock,
		};

		listenServer.new_connections_buffer.write(newConnection);
	}
}
