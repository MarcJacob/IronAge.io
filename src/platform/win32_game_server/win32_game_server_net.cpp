// Implementation file for the net communications component of the Win32 Game Server platform.

#include <WinSock2.h>
#include <ws2tcpip.h>

#include "win32_game_server_platform.h"

// Defines an active connection from the platform's perspective.
// A connection is considered "active" when:
// - Peer is still connected
// - Peer has data left to read
// - Peer has disconnected and and has no data left to read but the game server hasn't acknowledged the disconnection yet.
// The point is to respect the game server platform API instructions of providing the game server with a sequential view of connections
// even if, for example it was immediatelly closed, so the actual state of the platform connection does not always equate the actual state of the network connection.
struct win32_active_connection
{
	enum class STATE : i8
	{
		EMPTY, // Used to mark the connection structure itself as not representing any real connection.
		CONNECTED, // Connection was established, awaiting acknowledgement by the game server.
		OPEN, // Peer is still connected, and acknowledged by the game server.
		PEER_CLOSED, // Peer has disconnected, data is left to read and / or the connection wasn't acknowledged by the game server yet.
		SERVER_CLOSED, // Server requested this connection to be terminated.
		CLOSED, // Peer / platform ended the connection, no data is left to read, awaiting game server acknowledgement before cleanup.
		ENDED, // Connection is closed and has been acknowledged as closed by the game server. Undergoing cleanup.
	} state;

	SOCKET socket; // Win32 TCP socket. Set to INVALID_SOCKET when peer or server closes connection.

	// Set by the send thread while it is using the socket. The reception thread (which owns closing sockets) waits for it to clear
	// after moving the connection out of the OPEN state and before closing the socket.
	volatile i8 sending_data;

	ui32 address; // Address this connection is identified to, allowing multiple subsequent connections to be tied to the same peer.
	ui16 port; // Platform port used by this connection.

	// NOTE(Marc): For now, we just assign a separate reception & send buffer to each active connection, which may be a little wasteful.
	// Then again, this is made for a specific purpose: a game where nearly every connection is a game client, each with a similar load.
	// Later we can see if it's worth switching to a single reception / send buffer with the active connections just doing some bookeeping on
	// how much data they have awaiting reception.
	win32_single_ring_buffer<ui8> reception_buffer;
	win32_single_ring_buffer<ui8> send_buffer;
};

// Define win32 net component data.
struct win32_net_component
{
	WSAData wsa_data;

	mem_arena component_memory;

	struct
	{
		SOCKET socket;
		DWORD thread_id;
		HANDLE thread_handle;

	} listen_server;

	struct
	{
		DWORD id;
		HANDLE handle;
	} reception_thread;

	struct
	{
		DWORD id;
		HANDLE handle;
	} send_thread;

	// All threads access this in a specific pattern based on active connection state, avoiding race conditions until we decide to have more than one thread for each role.
	win32_active_connection* active_connections_table;

	// Is this component in the process of shutting down ? Set by the win32_net_stop function.
	volatile bool _shutdown_requested;
};

static win32_net_component WIN32_NET;

static constexpr ui64 NET_COMPONENT_MEMORY_SIZE = MiB(512); // Needs to be large enough to hold all the component buffers.

static constexpr ui16 MAX_ACTIVE_CONNECTIONS = 1024;
static constexpr ui16 ACTIVE_CONNECTION_RECEPTION_BUFFERS_SIZE = KiB(16);
static constexpr ui16 ACTIVE_CONNECTION_SEND_BUFFERS_SIZE = KiB(16);

// Thread functions declarations

bool create_listen_socket();
unsigned long listen_thread_func(LPVOID context);
unsigned long reception_thread_func(LPVOID context);
unsigned long send_thread_func(LPVOID context);

// ....

// Gets the active connection structure related to a handle, if any.
win32_active_connection& win32_get_active_connection(win32_connection_handle handle)
{
	ASSERT(handle < MAX_ACTIVE_CONNECTIONS);
	return WIN32_NET.active_connections_table[handle];
}

void win32_build_active_connections_table()
{
	auto& componentMemory = WIN32_NET.component_memory;

	WIN32_NET.active_connections_table = componentMemory.alloc<win32_active_connection>(MAX_ACTIVE_CONNECTIONS);
	auto& connectionsTable = WIN32_NET.active_connections_table;

	// Assign a reception and send buffer to each connection, allocated from component memory.
	for (ui16 connectionIndex = 0; connectionIndex < MAX_ACTIVE_CONNECTIONS; connectionIndex++)
	{
		win32_active_connection& connection = connectionsTable[connectionIndex];
		connection.socket = INVALID_SOCKET;

		connection.reception_buffer = win32_create_single_ring_buffer<ui8>(componentMemory, ACTIVE_CONNECTION_RECEPTION_BUFFERS_SIZE);
		ASSERT(connection.reception_buffer._mem != nullptr);

		connection.send_buffer = win32_create_single_ring_buffer<ui8>(componentMemory, ACTIVE_CONNECTION_SEND_BUFFERS_SIZE);
		ASSERT(connection.send_buffer._mem != nullptr);
	}
}

void win32_net_start()
{
	win32_log_stdout("Win32 Net: Starting Win32 Net component...");

	WIN32_NET = {};

	if (WSAStartup(MAKEWORD(2, 2), &WIN32_NET.wsa_data) != 0)
	{
		win32_logf_stderr("Win32 Net: Failed to start WSA. Error code = %d", WSAGetLastError());
		return;
	}

	// Allocate component memory.
	ui8* component_mem = (ui8*)VirtualAlloc(NULL, NET_COMPONENT_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WIN32_NET.component_memory = mem_arena_create(component_mem, NET_COMPONENT_MEMORY_SIZE);

	// Create active connections table.
	win32_build_active_connections_table();

	// Create listen server thread.
	if (!create_listen_socket())
	{
		win32_logf_stderr("Win32 Net: Failed to create listen socket.");
		win32_net_stop();
		return;
	}

	WIN32_NET.listen_server.thread_handle = CreateThread(NULL, NULL, listen_thread_func, nullptr, NULL, &WIN32_NET.listen_server.thread_id);
	if (WIN32_NET.listen_server.thread_handle == NULL)
	{
		win32_logf_stderr("Win32 Net: Failed to start listen server thread. Error code = %d", GetLastError());
		win32_net_stop();
		return;
	}

	// Create reception thread.
	WIN32_NET.reception_thread.handle = CreateThread(NULL, NULL, reception_thread_func, nullptr, NULL, &WIN32_NET.reception_thread.id);
	if (WIN32_NET.reception_thread.handle == NULL)
	{
		win32_logf_stderr("Win32 Net: Failed to start reception thread. Error code = %d", GetLastError());
		win32_net_stop();
		return;
	}

	// Create send thread.
	WIN32_NET.send_thread.handle = CreateThread(NULL, NULL, send_thread_func, nullptr, NULL, &WIN32_NET.send_thread.id);
	if (WIN32_NET.send_thread.handle == NULL)
	{
		win32_logf_stderr("Win32 Net: Failed to start send thread. Error code = %d", GetLastError());
		win32_net_stop();
		return;
	}
}

void win32_net_update_connections()
{
	// Reset connections marked as ENDED.
	auto& connectionsTable = WIN32_NET.active_connections_table;
	for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS; connectionHandle++)
	{
		win32_active_connection& activeConnection = win32_get_active_connection(connectionHandle);
		if (activeConnection.state != win32_active_connection::STATE::ENDED) continue;

		// Cleanup !

		// Reset buffers.
		activeConnection.reception_buffer.clear();
		activeConnection.send_buffer.clear();

		// Set the socket back to empty.
		InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::EMPTY);

		win32_logf_stdout("Win32 Net: Active connection handle %d is cleared and ready for re-use.", connectionHandle);
	}
}

void win32_net_stop()
{
	win32_log_stdout("Win32 Net: Shutting down Win32 net component...");

	WIN32_NET._shutdown_requested = true;

	// Listen thread is likely blocking on accept(), so close its socket to get it to wake up.
	closesocket(WIN32_NET.listen_server.socket);

	// Join secondary threads as they go through their shutdown routine.
	WaitForSingleObject(WIN32_NET.listen_server.thread_handle, INFINITE);
	WaitForSingleObject(WIN32_NET.reception_thread.handle, INFINITE);
	WaitForSingleObject(WIN32_NET.send_thread.handle, INFINITE);

	WSACleanup();
}

ui16 win32_net_query_new_connections(game_server_platform::in_connection* new_connections, ui16 buff_size)
{
	ASSERT(new_connections != nullptr && buff_size > 0);

	ui16 writeCount = 0;

	// NOTE(Marc): Going over the entire active connections table is simple but it may be too slow if we ever want this platform code to
	// handle connections in the tens of thousands or more. But since this is for a game server which is supposed to be *relatively* granular,
	// I'm not too concerned.
	// Still, leaving a static assert in here to start worrying about this if someone starts increasing the max active connections to something too high :)
	static_assert(MAX_ACTIVE_CONNECTIONS < 2048, "Win32 Net: Query New Connections function may be too slow to work for a large number of concurrent connections.");

	auto& connectionsTable = WIN32_NET.active_connections_table;
	for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS 
		&& writeCount < buff_size; connectionHandle++)
	{
		win32_active_connection& activeConnection = win32_get_active_connection(connectionHandle);
		if (activeConnection.state != win32_active_connection::STATE::CONNECTED) continue;

		// State transition due to acknowledgement (CONNECTED -> OPEN).
		InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::OPEN);

		win32_logf_stdout("Connection handle %d acknowledged by game server.", connectionHandle);

		// Write to buffer.
		new_connections[writeCount++] = {
			.platform_handle = connectionHandle,
			.address = activeConnection.address,
			.port = activeConnection.port,
		};
	}
	
	return writeCount;
}

ui16 win32_net_query_closed_connections(win32_connection_handle* closed_connections, ui16 buff_size)
{
	ASSERT(closed_connections != nullptr && buff_size > 0);

	ui16 writeCount = 0;

	// NOTE(Marc): Going over the entire active connections table is simple but it may be too slow if we ever want this platform code to
	// handle connections in the tens of thousands or more. But since this is for a game server which is supposed to be *relatively* granular,
	// I'm not too concerned.
	// Still, leaving a static assert in here to start worrying about this if someone starts increasing the max active connections to something too high :)
	static_assert(MAX_ACTIVE_CONNECTIONS < 2048, "Win32 Net: Query Closed Connections function may be too slow to work for a large number of concurrent connections.");

	auto& connectionsTable = WIN32_NET.active_connections_table;
	for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS 
		&& writeCount < buff_size; connectionHandle++)
	{
		win32_active_connection& activeConnection = win32_get_active_connection(connectionHandle);
		if (activeConnection.state != win32_active_connection::STATE::CLOSED) continue;

		// State transition due to acknowledgement (CLOSED -> ENDED).
		InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::ENDED);

		// Write to buffer.
		closed_connections[writeCount++] = connectionHandle;
	}
	
	return writeCount;
}

bool win32_net_send_bytes(win32_connection_handle connectionHandle, const ui8* bytes, ui32 byte_count)
{
	win32_active_connection& activeConnection = win32_get_active_connection(connectionHandle);
	if (activeConnection.state != win32_active_connection::STATE::OPEN) return false;

	// All or nothing, a partial write would corrupt the byte stream. We're the only producer so free capacity can only grow under us.
	if (activeConnection.send_buffer.get_free_capacity() < byte_count) return false;

	activeConnection.send_buffer.write(bytes, byte_count);
	return true;
}

ui32 win32_net_receive_bytes(win32_connection_handle connectionHandle, ui8* buffer, ui32 buff_size)
{
	win32_active_connection& activeConnection = win32_get_active_connection(connectionHandle);
	return activeConnection.reception_buffer.read(buffer, buff_size);
}

void win32_net_close_connection(win32_connection_handle connectionHandle)
{
	win32_active_connection& activeConnection = win32_get_active_connection(connectionHandle);
	InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::SERVER_CLOSED);
}

void win32_register_new_connection(win32_in_connection& new_connection)
{
	auto& connectionsTable = WIN32_NET.active_connections_table;

	// Connection sockets are non-blocking so a full send buffer can never stall the send thread (both threads poll before recv / send).
	u_long nonBlocking = 1;
	if (ioctlsocket(new_connection.socket, FIONBIO, &nonBlocking) == SOCKET_ERROR)
	{
		win32_logf_stderr("Win32 Net: Failed to set connection socket to non-blocking (Socket = %llu). Dropping connection.", new_connection.socket);
		closesocket(new_connection.socket);
		return;
	}

	// Find a free spot in the table. Log an error and close connection immediately if there's no room.
	for (win32_connection_handle newConnectionHandle = 0; newConnectionHandle < MAX_ACTIVE_CONNECTIONS; newConnectionHandle++)
	{
		win32_active_connection& activeConnectionEntry = connectionsTable[newConnectionHandle];
		if (activeConnectionEntry.state != win32_active_connection::STATE::EMPTY) continue;

		char addrBuff[128];
		inet_ntop(AF_INET, &new_connection.address, addrBuff, sizeof(addrBuff));

		win32_logf_stdout("Win32 Net: Registering new connection. Handle = %d\n\tAddress = %s\n\tPort = %d", 
			newConnectionHandle, addrBuff, new_connection.port);

		activeConnectionEntry.socket = new_connection.socket;
		activeConnectionEntry.address = new_connection.address;
		activeConnectionEntry.port = new_connection.port;

		// State transition EMPTY -> CONNECTED
		InterlockedExchange8((i8*)&activeConnectionEntry.state, (i8)win32_active_connection::STATE::CONNECTED);
		return;
	}

	// No room in the table.
	win32_logf_stderr("Win32 Net: Max active connections capacity reached ! Dropping connection (Socket = %llu).", new_connection.socket);
	closesocket(new_connection.socket);
}

bool create_listen_socket()
{
	auto& listenServer = WIN32_NET.listen_server;

	listenServer.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenServer.socket == INVALID_SOCKET)
	{
		win32_log_stderr("Win32 Net: Failed to create listen server socket.");
		return false;
	}

	struct sockaddr_in bind_addr = {};
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(8000);
	bind_addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);

	if (bind(listenServer.socket, (sockaddr*)&bind_addr, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		win32_logf_stderr("Win32 Net: Failed to bind listen server socket. WSA error = %d", WSAGetLastError());
		closesocket(listenServer.socket);
		listenServer.socket = INVALID_SOCKET;
		return false;
	}
	
	if (listen(listenServer.socket, 256) == SOCKET_ERROR)
	{
		win32_log_stderr("Win32 Net: Failed to start listening on listen server socket.");	
		closesocket(listenServer.socket);
		listenServer.socket = INVALID_SOCKET;
		return false;
	}

	return true;
}

// Runs a listen server on loop, accepting new connections adding them to the active connections table.
unsigned long listen_thread_func(LPVOID context)
{
	auto& listenServer = WIN32_NET.listen_server;

	sockaddr_in newConnectionAddr = {};
	i32 addrLen = sizeof(newConnectionAddr);

	// Main loop
	for (;;)
	{
		newConnectionAddr = {};

		addrLen = sizeof(newConnectionAddr);
		SOCKET newSock = accept(listenServer.socket, (sockaddr*)&newConnectionAddr, &addrLen);
		if (newSock == INVALID_SOCKET)
		{
			// "Catch" shutdown request here. The socket will have been closed by main shutdown routine.
			if (WIN32_NET._shutdown_requested)
			{
				return 0;
			}

			i32 err = WSAGetLastError();
			win32_logf_stderr("Win32 Net: Error accepting new connection on listen server. Error code = %d", err);

			// Check if error is fatal.
			if (err == WSAEINTR || err == WSAENOTSOCK)
			{
				// End thread with code 1.
				closesocket(listenServer.socket);
				listenServer.socket = INVALID_SOCKET;
				return 1;
			}
			continue;
		}

		win32_in_connection newConnection = {
			.address = newConnectionAddr.sin_addr.S_un.S_addr,
			.port = ntohs(newConnectionAddr.sin_port),
			.socket = newSock,
		};

		win32_register_new_connection(newConnection);
	}
}

// Closes the socket of an active connection, if it still has one.
// Reception thread only, and only once the connection's state has left OPEN so the send thread stops starting new sends on it.
void close_connection_socket(win32_active_connection& connection)
{
	while (connection.sending_data) YieldProcessor();

	if (connection.socket != INVALID_SOCKET)
	{
		closesocket(connection.socket);
		connection.socket = INVALID_SOCKET;
	}
}

// Runs reception on all active connections with their socket still open,
// and performs OPEN->PEER_CLOSED, SERVER_CLOSED->CLOSED and PEER_CLOSED->CLOSED transitions.
unsigned long reception_thread_func(LPVOID context)
{
	WSAPOLLFD pollBuff[MAX_ACTIVE_CONNECTIONS] = {0};
	win32_connection_handle pollToActiveConnectionHandle[MAX_ACTIVE_CONNECTIONS];

RECEPTION_THREAD_START:

	ui16 pollCount = 0;

	// First pass: SERVER_CLOSED transitions to CLOSED, and build poll buffer.
	auto& connectionsTable = WIN32_NET.active_connections_table;
	for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS; connectionHandle++)
	{
		win32_active_connection& activeConnection = connectionsTable[connectionHandle];

		// If server requested this connection to be closed, do so here.
		if (activeConnection.state == win32_active_connection::STATE::SERVER_CLOSED)
		{
			// Close the socket, and perform the transition to CLOSED.
			win32_logf_stdout("Win32 NET: Connection handle %d closed by request of server.", connectionHandle);

			close_connection_socket(activeConnection);

			InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::CLOSED);
			continue;
		}

		// Only act upon connections OPEN. CONNECTED connections will start buffering data on the OS side.
		if (activeConnection.state != win32_active_connection::STATE::OPEN) continue;

		// Skip connections whose reception buffer is already full.
		if (activeConnection.reception_buffer.get_free_capacity() == 0) continue;

		pollBuff[pollCount].fd = activeConnection.socket;
		pollBuff[pollCount].events = POLLRDNORM | POLLRDBAND;
		pollToActiveConnectionHandle[pollCount] = connectionHandle;

		pollCount++;
	}

	// Perform polling if at least one connection needs it.
	if (pollCount == 0)
	{
		Sleep(1);
	}
	else
	{
		i32 recvCount = WSAPoll(pollBuff, pollCount, 5);

		constexpr ui16 RECEPTION_BUFFER_SIZE = 1024;
		i8 RECEPTION_BUFFER[RECEPTION_BUFFER_SIZE] = {0}; // Reception buffer before copy into the reception ring buffer. 
		for (i32 pollIndex = 0; recvCount > 0 && pollIndex < pollCount; pollIndex++)
		{
			win32_connection_handle& recvConnectionHandle = pollToActiveConnectionHandle[pollIndex];
			win32_active_connection& recvConnection = connectionsTable[recvConnectionHandle];
			WSAPOLLFD& pollFD = pollBuff[pollIndex];

			ASSERT((pollFD.revents & POLLNVAL) == 0); // Assert that we are always, at least, using a correct socket value.

			if (pollFD.revents & POLLERR)
			{
				win32_logf_stderr("Win32 Net: Error polling socket state on connection handle %d (SOCKET = %llu), closing connection.",
					recvConnectionHandle, recvConnection.socket);

				// Set connection to SERVER CLOSED and close socket.
				InterlockedExchange8((i8*)&recvConnection.state, (i8)win32_active_connection::STATE::SERVER_CLOSED);
				close_connection_socket(recvConnection);
				continue;
			}

			if (pollFD.revents & POLLHUP)
			{
				// Connection aborted. Consider it as "closed by peer".
				win32_logf_stdout("Win32 NET: Connection handle %d closed by peer.", recvConnectionHandle);

				InterlockedExchange8((i8*)&recvConnection.state, (i8)win32_active_connection::STATE::PEER_CLOSED);
				close_connection_socket(recvConnection);
				continue;
			}

			if (pollFD.revents & (POLLRDNORM | POLLRDBAND))
			{
				// Attempt to receive bytes from the connection. Detect closures / close socket.
				i32 receptionMaxSize = (i32)recvConnection.reception_buffer.get_free_capacity(); // Assume that the ring buffer is no larger than half of i32 max value.
				receptionMaxSize = receptionMaxSize > RECEPTION_BUFFER_SIZE ? RECEPTION_BUFFER_SIZE : receptionMaxSize;

				i32 res = recv(recvConnection.socket, RECEPTION_BUFFER, receptionMaxSize, NULL);
				if (res == SOCKET_ERROR)
				{
					// Non-blocking socket with nothing to read after all, not an error.
					if (WSAGetLastError() == WSAEWOULDBLOCK) continue;

					win32_logf_stderr("Win32 Net: Error code %d receiving data socket state on connection handle %d (SOCKET = %llu), closing connection.",
						WSAGetLastError(), recvConnectionHandle, recvConnection.socket);

					// Set connection to SERVER CLOSED and close socket immediately.
					InterlockedExchange8((i8*)&recvConnection.state, (i8)win32_active_connection::STATE::SERVER_CLOSED);
					close_connection_socket(recvConnection);
					continue;
				}

				if (res == 0)
				{
					// Socket closed connection gracefully. Transition state to PEER_CLOSED and close the socket.
					win32_logf_stdout("Win32 NET: Connection handle %d closed by peer.", recvConnectionHandle);

					InterlockedExchange8((i8*)&recvConnection.state, (i8)win32_active_connection::STATE::PEER_CLOSED);
					close_connection_socket(recvConnection);
					continue;
				}

				// Push received data to ring buffer.
				recvConnection.reception_buffer.write((ui8*)RECEPTION_BUFFER, res);
			}
		}
	}

	// Second pass: Find PEER_CLOSED connections, check their reception buffer. If it's empty, set them to closed for server acknowledgement.
	for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS; connectionHandle++)
	{
		win32_active_connection& activeConnection = connectionsTable[connectionHandle];

		if (activeConnection.state == win32_active_connection::STATE::PEER_CLOSED
			&& activeConnection.reception_buffer.item_count == 0) // We're the producer for it so there's no race condition to worry about.
		{
			win32_logf_stdout("Win32 Net: Closed-by-peer connection handle %d has no more data to read. Closing...", connectionHandle);
			InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::CLOSED);
		}
	}

	if (WIN32_NET._shutdown_requested)
	{
		// Close all active connection sockets.
		for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS; connectionHandle++)
		{
			win32_active_connection& connection = win32_get_active_connection(connectionHandle);
			if (connection.socket == INVALID_SOCKET) continue;

			// Leave the OPEN state first so the send thread stops using the socket.
			InterlockedExchange8((i8*)&connection.state, (i8)win32_active_connection::STATE::CLOSED);
			close_connection_socket(connection);
		}

		return 0;
	}
	goto RECEPTION_THREAD_START;
}

// Sends buffered data on all OPEN connections that have some, and are ready to accept it.
// Only ever consumes from the connections' send buffers, and never closes sockets (that is the reception thread's job).
unsigned long send_thread_func(LPVOID context)
{
	constexpr ui16 SEND_CHUNK_SIZE = 4096;

	WSAPOLLFD pollBuff[MAX_ACTIVE_CONNECTIONS] = {0};
	win32_connection_handle pollToActiveConnectionHandle[MAX_ACTIVE_CONNECTIONS];
	ui8 sendChunk[SEND_CHUNK_SIZE];

	auto& connectionsTable = WIN32_NET.active_connections_table;

	while (!WIN32_NET._shutdown_requested)
	{
		// First pass: build poll buffer.
		ui16 pollCount = 0;
		for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS; connectionHandle++)
		{
			win32_active_connection& activeConnection = connectionsTable[connectionHandle];

			if (activeConnection.state != win32_active_connection::STATE::OPEN) continue;
			if (activeConnection.send_buffer.item_count == 0) continue;

			SOCKET connectionSocket = activeConnection.socket;
			if (connectionSocket == INVALID_SOCKET) continue;

			pollBuff[pollCount].fd = connectionSocket;
			pollBuff[pollCount].events = POLLWRNORM;
			pollToActiveConnectionHandle[pollCount] = connectionHandle;

			pollCount++;
		}

		if (pollCount == 0)
		{
			Sleep(1);
			continue;
		}

		i32 readyCount = WSAPoll(pollBuff, pollCount, 5);

		// Second pass: send on writable sockets.
		for (i32 pollIndex = 0; readyCount > 0 && pollIndex < pollCount; pollIndex++)
		{
			WSAPOLLFD& pollFD = pollBuff[pollIndex];

			// Errors / hangups are left for the reception thread to detect and handle. The socket may also have been closed since the poll buffer was built.
			if ((pollFD.revents & POLLWRNORM) == 0) continue;

			win32_connection_handle sendConnectionHandle = pollToActiveConnectionHandle[pollIndex];
			win32_active_connection& sendConnection = connectionsTable[sendConnectionHandle];

			// Tell the reception thread we're about to use the socket.
			InterlockedExchange8(&sendConnection.sending_data, 1);
			if (sendConnection.state == win32_active_connection::STATE::OPEN)
			{
				ui64 chunkSize = sendConnection.send_buffer.peek(sendChunk, SEND_CHUNK_SIZE);
				if (chunkSize > 0)
				{
					i32 sentCount = send(sendConnection.socket, (i8*)sendChunk, (i32)chunkSize, NULL);
					if (sentCount != SOCKET_ERROR)
					{
						// Only consume what actually went out. The rest stays buffered for the next pass.
						sendConnection.send_buffer.discard(sentCount);
					}
					else if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						// Leave the connection alone, the reception thread will detect the failure and close it.
						win32_logf_stderr("Win32 Net: Error code %d sending data on connection handle %d (SOCKET = %llu).",
							WSAGetLastError(), sendConnectionHandle, sendConnection.socket);
					}
				}
			}
			InterlockedExchange8(&sendConnection.sending_data, 0);
		}
	}

	return 0;
}
