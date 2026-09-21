// Implementation file for the net communications component of the Win32 Game Server platform.

#include <WinSock2.h>
#include <ws2tcpip.h>

#include "win32_game_server_platform.h"

// Net component logging: goes through the Win32 platform logging under the "NET" component.
static inline void net_log(LOG_TYPE type, const char* msg) { win32_log("NET", type, msg); }
static inline void net_log(const char* msg) { win32_log("NET", msg); }
template<typename... args_types>
static void net_logf(LOG_TYPE type, const char* format, args_types... args) { win32_logf("NET", type, format, args...); }
template<typename... args_types>
static void net_logf(const char* format, args_types... args) { win32_logf("NET", format, args...); }

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

struct win32_active_connections_table
{
	win32_active_connection* connections;
	ui16 max_active_connections;
};

// Define win32 net component data.
struct win32_net_component
{
	bool active; // Whether the component is still going or has stopped (usually because of program shutdown or an error).

	WSAData wsa_data;

	mem_arena component_memory;

	struct listen_thread_context
	{
		win32_active_connections_table* connections_table; // Connections table to register new connections into.
		volatile SOCKET socket; // Socket to use for accepting new connections. Close to stop the thread.
		volatile bool err_signal; // Set to true by the thread if it encountered a fatal error.

		DWORD thread_id;
		HANDLE thread_handle;
	} listen_thread;

	struct reception_thread_context
	{
		win32_active_connections_table* connections_table; // Connections table to find sockets & bytes awaiting consumption.
		volatile bool stop_signal; // Set to true to stop the thread gracefully.
		volatile bool err_signal; // Set to true by the thread if it encountered a fatal error.

		DWORD thread_id;
		HANDLE thread_handle;
	} reception_thread;

	struct send_thread_context
	{
		win32_active_connections_table* connections_table; // Connections table to find sockets & bytes awaiting dispatch.
		volatile bool stop_signal; // Set to true to stop the thread gracefully.
		volatile bool err_signal; // Set to true by the thread if it encountered a fatal error.

		DWORD thread_id;
		HANDLE thread_handle;
	} send_thread;

	// All threads access this in a specific pattern based on active connection state, avoiding race conditions until we decide to have more than one thread for each role.
	win32_active_connections_table active_connections_table;
};

static constexpr ui64 NET_COMPONENT_MEMORY_SIZE = GiB(1); // Needs to be large enough to hold all the component buffers.

static constexpr ui16 MAX_ACTIVE_CONNECTIONS = 1024;
static constexpr ui16 ACTIVE_CONNECTION_RECEPTION_BUFFERS_SIZE = KiB(32);
static constexpr ui16 ACTIVE_CONNECTION_SEND_BUFFERS_SIZE = KiB(32);

SOCKET create_listen_socket();

unsigned long listen_thread_func(LPVOID context);
unsigned long reception_thread_func(LPVOID context);
unsigned long send_thread_func(LPVOID context);

// Gets the active connection structure related to a thread_handle, if any.
// Used to link a connection handle to an active connection structure, in places that aren't privy to how the mapping works exactly.
win32_active_connection& win32_net_get_active_connection(win32_net_component& net_component, win32_connection_handle connection_handle)
{
	ASSERT(connection_handle < net_component.active_connections_table.max_active_connections);

	return net_component.active_connections_table.connections[connection_handle];
}

void win32_build_active_connections_table(win32_net_component& net_component, ui16 max_active_connections)
{
	auto& componentMemory = net_component.component_memory;

	net_component.active_connections_table.connections = componentMemory.alloc<win32_active_connection>(max_active_connections);
	net_component.active_connections_table.max_active_connections = max_active_connections;

	auto& connectionsTable = net_component.active_connections_table;

	// Assign a reception and send buffer to each connection, allocated from component memory.
	for (ui16 connectionIndex = 0; connectionIndex < MAX_ACTIVE_CONNECTIONS; connectionIndex++)
	{
		win32_active_connection& connection = connectionsTable.connections[connectionIndex];
		connection.socket = INVALID_SOCKET;

		connection.reception_buffer = win32_create_single_ring_buffer<ui8>(componentMemory, ACTIVE_CONNECTION_RECEPTION_BUFFERS_SIZE);
		ASSERT(connection.reception_buffer._mem != nullptr);

		connection.send_buffer = win32_create_single_ring_buffer<ui8>(componentMemory, ACTIVE_CONNECTION_SEND_BUFFERS_SIZE);
		ASSERT(connection.send_buffer._mem != nullptr);
	}
}

SOCKET create_listen_socket()
{
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET)
	{
		net_log(LOG_ERROR, "Failed to create listen server socket.");
		return INVALID_SOCKET;
	}

	struct sockaddr_in bind_addr = {};
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(8000);
	bind_addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);

	if (bind(listenSocket, (sockaddr*)&bind_addr, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		net_logf(LOG_ERROR, "Failed to bind listen server socket. WSA error = %d", WSAGetLastError());
		closesocket(listenSocket);
		return INVALID_SOCKET;
	}

	if (listen(listenSocket, 256) == SOCKET_ERROR)
	{
		net_logf(LOG_ERROR, "Failed to start listening on listen server socket. WSA error = %d", WSAGetLastError());
		closesocket(listenSocket);
		return INVALID_SOCKET;
	}

	return listenSocket;
}

win32_net_component* win32_net_start()
{
	net_log("Starting net component...");

	// Allocate component memory directly from the OS.
	ui8* component_mem = (ui8*)VirtualAlloc(NULL, NET_COMPONENT_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	ASSERT(component_mem);

	mem_arena component_memory = mem_arena_create(component_mem, NET_COMPONENT_MEMORY_SIZE);

	win32_net_component* netComponent = component_memory.alloc<win32_net_component>();
	ASSERT(netComponent != nullptr);

	netComponent->component_memory = component_memory;

	if (WSAStartup(MAKEWORD(2, 2), &netComponent->wsa_data) != 0)
	{
		net_logf(LOG_ERROR, "Failed to start WSA. Error code = %d", WSAGetLastError());
		win32_net_stop(*netComponent);
		return nullptr;
	}

	// Create active connections table.
	win32_build_active_connections_table(*netComponent, MAX_ACTIVE_CONNECTIONS);

	// Create listen server thread.
	SOCKET listenSocket = create_listen_socket();
	if (listenSocket == INVALID_SOCKET)
	{
		net_log(LOG_ERROR, "Failed to create listen socket.");
		win32_net_stop(*netComponent);
		return nullptr;
	}

	// Create listen thread context and start it.
	netComponent->listen_thread = {
		.connections_table = &netComponent->active_connections_table,
		.socket = listenSocket,

		.thread_handle = CreateThread(NULL, NULL, listen_thread_func, &netComponent->listen_thread,
		CREATE_SUSPENDED, &netComponent->listen_thread.thread_id),
	};

	if (netComponent->listen_thread.thread_handle == NULL)
	{
		net_logf(LOG_ERROR, "Failed to start listen server thread. Error code = %d", GetLastError());
		win32_net_stop(*netComponent);
		return nullptr;
	}
	ResumeThread(netComponent->listen_thread.thread_handle);

	// Create reception thread context and start it.
	netComponent->reception_thread = {
		.connections_table = &netComponent->active_connections_table,
		.thread_handle = CreateThread(NULL, NULL, reception_thread_func, &netComponent->reception_thread,
		CREATE_SUSPENDED, &netComponent->reception_thread.thread_id),
	};

	if (netComponent->reception_thread.thread_handle == NULL)
	{
		net_logf(LOG_ERROR, "Failed to start reception thread. Error code = %d", GetLastError());
		win32_net_stop(*netComponent);
		return nullptr;
	}
	ResumeThread(netComponent->reception_thread.thread_handle);

	// Create send thread context and start it.
	netComponent->send_thread = {
		.connections_table = &netComponent->active_connections_table,
		.thread_handle = CreateThread(NULL, NULL, send_thread_func, &netComponent->send_thread,
			CREATE_SUSPENDED, &netComponent->send_thread.thread_id), 
	};

	if (netComponent->send_thread.thread_handle == NULL)
	{
		net_logf(LOG_ERROR, "Failed to start send thread. Error code = %d", GetLastError());
		win32_net_stop(*netComponent);
		return nullptr;
	}
	ResumeThread(netComponent->send_thread.thread_handle);

	netComponent->active = true;
	return netComponent;
}

void win32_net_update_connections(win32_net_component& net_component)
{
	ASSERT(net_component.active);

	// Check health of service threads, and stop the net component if any of them have stopped.
	if (net_component.listen_thread.err_signal
		|| net_component.reception_thread.err_signal
		|| net_component.send_thread.err_signal)
	{
		// Assume the thread has already logged its error. We just care about stopping component activity as soon as possible.
		net_component.active = false; // Set active to false and let the main platform code take the component down.
		return;
	}

	// Reset connections marked as ENDED.
	for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS; connectionHandle++)
	{
		win32_active_connection& activeConnection = win32_net_get_active_connection(net_component, connectionHandle);
		if (activeConnection.state != win32_active_connection::STATE::ENDED) continue;

		// Cleanup !

		// Reset buffers.
		activeConnection.reception_buffer.clear();
		activeConnection.send_buffer.clear();

		// Set the socket back to empty.
		InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::EMPTY);

		net_logf("Active connection thread_handle %d is cleared and ready for re-use.", connectionHandle);
	}
}

void win32_net_stop(win32_net_component& net_component)
{
	net_log("Shutting down net component...");

	net_component.active = false;

	// Signal service threads to stop.
	closesocket(net_component.listen_thread.socket), net_component.listen_thread.socket = INVALID_SOCKET;
	net_component.reception_thread.stop_signal = true;
	net_component.send_thread.stop_signal = true;

	// Join service threads.
	WaitForSingleObject(net_component.listen_thread.thread_handle, INFINITE);
	WaitForSingleObject(net_component.reception_thread.thread_handle, INFINITE);
	WaitForSingleObject(net_component.send_thread.thread_handle, INFINITE);

	WSACleanup();
}

void win32_net_free(win32_net_component& net_component)
{
	VirtualFree(net_component.component_memory.mem_start, net_component.component_memory.mem_size, MEM_RELEASE | MEM_FREE);
	net_component = {};
}

// BEGIN PLATFORM NET INTERFACE FUNCTIONS

ui16 win32_net_query_new_connections(game_server_platform& platform, game_server_platform::in_connection* new_connections, ui16 buff_size)
{
	ASSERT(new_connections != nullptr && buff_size > 0);

	win32_net_component& netComponent = *((win32_platform&)platform).net_component;
	if (netComponent.active == false) return 0;

	ui16 writeCount = 0;

	// NOTE(Marc): Going over the entire active connections table is simple but it may be too slow if we ever want this platform code to
	// thread_handle connections in the tens of thousands or more. But since this is for a game server which is supposed to be *relatively* granular,
	// I'm not too concerned.
	// Still, leaving a static assert in here to start worrying about this if someone starts increasing the max active connections to something too high :)
	static_assert(MAX_ACTIVE_CONNECTIONS < 2048, "Win32 Net: Query New Connections function may be too slow to work for a large number of concurrent connections.");

	for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS 
		&& writeCount < buff_size; connectionHandle++)
	{
		win32_active_connection& activeConnection = win32_net_get_active_connection(netComponent, connectionHandle);
		if (activeConnection.state != win32_active_connection::STATE::CONNECTED) continue;

		// State transition due to acknowledgement (CONNECTED -> OPEN).
		InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::OPEN);

		net_logf("Connection thread_handle %d acknowledged by game server.", connectionHandle);

		// Write to buffer.
		new_connections[writeCount++] = {
			.platform_handle = connectionHandle,
			.address = activeConnection.address,
			.port = activeConnection.port,
		};
	}
	
	return writeCount;
}

ui16 win32_net_query_closed_connections(game_server_platform& platform, win32_connection_handle* closed_connections, ui16 buff_size)
{
	ASSERT(closed_connections != nullptr && buff_size > 0);

	win32_net_component& netComponent = *((win32_platform&)platform).net_component;
	if (netComponent.active == false) return 0;

	ui16 writeCount = 0;

	// NOTE(Marc): Going over the entire active connections table is simple but it may be too slow if we ever want this platform code to
	// thread_handle connections in the tens of thousands or more. But since this is for a game server which is supposed to be *relatively* granular,
	// I'm not too concerned.
	// Still, leaving a static assert in here to start worrying about this if someone starts increasing the max active connections to something too high :)
	static_assert(MAX_ACTIVE_CONNECTIONS < 2048, "Win32 Net: Query Closed Connections function may be too slow to work for a large number of concurrent connections.");

	for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS 
		&& writeCount < buff_size; connectionHandle++)
	{
		win32_active_connection& activeConnection = win32_net_get_active_connection(netComponent, connectionHandle);
		if (activeConnection.state != win32_active_connection::STATE::CLOSED) continue;

		// State transition due to acknowledgement (CLOSED -> ENDED).
		InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::ENDED);

		// Write to buffer.
		closed_connections[writeCount++] = connectionHandle;
	}
	
	return writeCount;
}

bool win32_net_send_bytes(game_server_platform& platform, win32_connection_handle connectionHandle, const ui8* bytes, ui32 byte_count)
{
	win32_net_component& netComponent = *((win32_platform&)platform).net_component;
	if (netComponent.active == false) return false;

	win32_active_connection& activeConnection = win32_net_get_active_connection(netComponent, connectionHandle);
	if (activeConnection.state != win32_active_connection::STATE::OPEN) return false;

	// All or nothing, a partial write would corrupt the byte stream. We're the only producer so free capacity can only grow under us.
	if (activeConnection.send_buffer.get_free_capacity() < byte_count) return false;

	activeConnection.send_buffer.write(bytes, byte_count);
	return true;
}

ui32 win32_net_receive_bytes(game_server_platform& platform, win32_connection_handle connectionHandle, ui8* buffer, ui32 buff_size)
{
	win32_net_component& netComponent = *((win32_platform&)platform).net_component;
	if (netComponent.active == false) return 0;

	win32_active_connection& activeConnection = win32_net_get_active_connection(netComponent, connectionHandle);
	return activeConnection.reception_buffer.read(buffer, buff_size);
}

void win32_net_close_connection(game_server_platform& platform, win32_connection_handle connectionHandle)
{
	win32_net_component& netComponent = *((win32_platform&)platform).net_component;
	if (netComponent.active == false) return;

	win32_active_connection& activeConnection = win32_net_get_active_connection(netComponent, connectionHandle);
	InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::SERVER_CLOSED);
}

// END PLATFORM NET INTERFACE FUNCTIONS

// BEGIN LISTEN THREAD

// Registers a new connection into the given connections table.
void win32_register_new_connection(win32_active_connections_table& connections_table, win32_in_connection& new_connection)
{
	// Connection sockets are non-blocking so a full send buffer can never stall the send thread (both threads poll before recv / send).
	u_long nonBlocking = 1;
	if (ioctlsocket(new_connection.socket, FIONBIO, &nonBlocking) == SOCKET_ERROR)
	{
		net_logf(LOG_ERROR, "Failed to set connection socket to non-blocking (Socket = %llu). Dropping connection.", new_connection.socket);
		closesocket(new_connection.socket);
		return;
	}

	// Find a free spot in the table. Log an error and close connection immediately if there's no room.
	for (win32_connection_handle newConnectionHandle = 0; newConnectionHandle < MAX_ACTIVE_CONNECTIONS; newConnectionHandle++)
	{
		win32_active_connection& activeConnectionEntry = connections_table.connections[newConnectionHandle];
		if (activeConnectionEntry.state != win32_active_connection::STATE::EMPTY) continue;

		char addrBuff[128];
		inet_ntop(AF_INET, &new_connection.address, addrBuff, sizeof(addrBuff));

		net_logf("Registering new connection. Handle = %d\n\tAddress = %s\n\tPort = %d", 
			newConnectionHandle, addrBuff, new_connection.port);

		activeConnectionEntry.socket = new_connection.socket;
		activeConnectionEntry.address = new_connection.address;
		activeConnectionEntry.port = new_connection.port;

		// State transition EMPTY -> CONNECTED
		InterlockedExchange8((i8*)&activeConnectionEntry.state, (i8)win32_active_connection::STATE::CONNECTED);
		return;
	}

	// No room in the table.
	net_logf(LOG_ERROR, "Max active connections capacity reached ! Dropping connection (Socket = %llu).", new_connection.socket);
	closesocket(new_connection.socket);
}

// Runs a listen server on loop, accepting new connections adding them to the active connections table.
unsigned long listen_thread_func(LPVOID context)
{
	win32_net_component::listen_thread_context& listenThread = *(win32_net_component::listen_thread_context*)context;
	ASSERT(listenThread.socket != INVALID_SOCKET);
	ASSERT(listenThread.connections_table != nullptr);

	sockaddr_in newConnectionAddr = {};
	i32 addrLen = sizeof(newConnectionAddr);

	// Main loop
	for (;;)
	{
		newConnectionAddr = {};

		addrLen = sizeof(newConnectionAddr);
		SOCKET newSock = accept(listenThread.socket, (sockaddr*)&newConnectionAddr, &addrLen);
		if (newSock == INVALID_SOCKET)
		{
			// "Catch" shutdown request here. The socket will have been closed and invalidated by main shutdown routine.
			if (listenThread.socket == INVALID_SOCKET)
			{
				return 0;
			}

			i32 err = WSAGetLastError();
			net_logf(LOG_ERROR, "Error accepting new connection on listen server. Error code = %d", err);

			// Check if error is fatal.
			if (err == WSAEINTR || err == WSAENOTSOCK)
			{
				net_log(LOG_ERROR, "Error is fatal ! Shutting down listen thread.");
				listenThread.err_signal = true;
				return 1;
			}
			continue;
		}

		win32_in_connection newConnection = {
			.address = newConnectionAddr.sin_addr.S_un.S_addr,
			.port = ntohs(newConnectionAddr.sin_port),
			.socket = newSock,
		};

		win32_register_new_connection(*listenThread.connections_table, newConnection);
	}
}

// END LISTEN THREAD

// BEGIN RECEPTION THREAD

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
	win32_net_component::reception_thread_context& receptionThread = *(win32_net_component::reception_thread_context*)context;

	WSAPOLLFD pollBuff[MAX_ACTIVE_CONNECTIONS] = {0};
	win32_connection_handle pollToActiveConnectionHandle[MAX_ACTIVE_CONNECTIONS];

RECEPTION_THREAD_START:

	ui16 pollCount = 0;

	// First pass: SERVER_CLOSED transitions to CLOSED, and build poll buffer.
	auto& connectionsTable = *receptionThread.connections_table;
	for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS; connectionHandle++)
	{
		win32_active_connection& activeConnection = connectionsTable.connections[connectionHandle];

		// If server requested this connection to be closed, do so here.
		if (activeConnection.state == win32_active_connection::STATE::SERVER_CLOSED)
		{
			// Let the send thread flush what the server queued before the close request. Nothing to flush if the socket is already gone.
			if (activeConnection.socket != INVALID_SOCKET && activeConnection.send_buffer.item_count > 0) continue;

			// Close the socket, and perform the transition to CLOSED.
			net_logf("Connection thread_handle %d closed by request of server.", connectionHandle);

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
			win32_active_connection& recvConnection = connectionsTable.connections[recvConnectionHandle];
			WSAPOLLFD& pollFD = pollBuff[pollIndex];

			ASSERT((pollFD.revents & POLLNVAL) == 0); // Assert that we are always, at least, using a correct socket value.

			if (pollFD.revents & POLLERR)
			{
				net_logf(LOG_ERROR, "Error polling socket state on connection thread_handle %d (SOCKET = %llu), closing connection.",
					recvConnectionHandle, recvConnection.socket);

				// Set connection to SERVER CLOSED and close socket.
				InterlockedExchange8((i8*)&recvConnection.state, (i8)win32_active_connection::STATE::SERVER_CLOSED);
				close_connection_socket(recvConnection);
				continue;
			}

			if (pollFD.revents & POLLHUP)
			{
				// Connection aborted. Consider it as "closed by peer".
				net_logf("Connection thread_handle %d closed by peer.", recvConnectionHandle);

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

					net_logf(LOG_ERROR, "Error code %d receiving data socket state on connection thread_handle %d (SOCKET = %llu), closing connection.",
						WSAGetLastError(), recvConnectionHandle, recvConnection.socket);

					// Set connection to SERVER CLOSED and close socket immediately.
					InterlockedExchange8((i8*)&recvConnection.state, (i8)win32_active_connection::STATE::SERVER_CLOSED);
					close_connection_socket(recvConnection);
					continue;
				}

				if (res == 0)
				{
					// Socket closed connection gracefully. Transition state to PEER_CLOSED and close the socket.
					net_logf("Connection thread_handle %d closed by peer.", recvConnectionHandle);

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
		win32_active_connection& activeConnection = connectionsTable.connections[connectionHandle];

		if (activeConnection.state == win32_active_connection::STATE::PEER_CLOSED
			&& activeConnection.reception_buffer.item_count == 0) // We're the producer for it so there's no race condition to worry about.
		{
			net_logf("Closed-by-peer connection thread_handle %d has no more data to read. Closing...", connectionHandle);
			InterlockedExchange8((i8*)&activeConnection.state, (i8)win32_active_connection::STATE::CLOSED);
		}
	}

	if (receptionThread.stop_signal)
	{
		// Close all active connection sockets.
		for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS; connectionHandle++)
		{
			win32_active_connection& connection = connectionsTable.connections[connectionHandle];
			if (connection.socket == INVALID_SOCKET) continue;

			// Leave the OPEN state first so the send thread stops using the socket.
			InterlockedExchange8((i8*)&connection.state, (i8)win32_active_connection::STATE::CLOSED);
			close_connection_socket(connection);
		}

		return 0;
	}
	goto RECEPTION_THREAD_START;
}

// END RECEPTION THREAD

// BEGIN SEND THREAD

// Sends buffered data on all OPEN (or SERVER_CLOSED, flushing before the close) connections that have some, and are ready to accept it.
// Only ever consumes from the connections' send buffers, and never closes sockets (that is the reception thread's job).
unsigned long send_thread_func(LPVOID context)
{
	win32_net_component::send_thread_context& sendThread = *(win32_net_component::send_thread_context*)context;

	constexpr ui16 SEND_CHUNK_SIZE = 4096;

	WSAPOLLFD pollBuff[MAX_ACTIVE_CONNECTIONS] = {0};
	win32_connection_handle pollToActiveConnectionHandle[MAX_ACTIVE_CONNECTIONS];
	ui8 sendChunk[SEND_CHUNK_SIZE];

	auto& connectionsTable = *sendThread.connections_table;

	while (!sendThread.stop_signal)
	{
		// First pass: build poll buffer.
		ui16 pollCount = 0;
		for (win32_connection_handle connectionHandle = 0; connectionHandle < MAX_ACTIVE_CONNECTIONS; connectionHandle++)
		{
			win32_active_connection& activeConnection = connectionsTable.connections[connectionHandle];

			if (activeConnection.state != win32_active_connection::STATE::OPEN
				&& activeConnection.state != win32_active_connection::STATE::SERVER_CLOSED) continue;
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

			// Errors / hangups are left for the reception thread to detect and thread_handle. The socket may also have been closed since the poll buffer was built.
			if ((pollFD.revents & POLLWRNORM) == 0) continue;

			win32_connection_handle sendConnectionHandle = pollToActiveConnectionHandle[pollIndex];
			win32_active_connection& sendConnection = connectionsTable.connections[sendConnectionHandle];

			// Tell the reception thread we're about to use the socket.
			InterlockedExchange8(&sendConnection.sending_data, 1);
			if (sendConnection.state == win32_active_connection::STATE::OPEN
				|| sendConnection.state == win32_active_connection::STATE::SERVER_CLOSED)
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
						net_logf(LOG_ERROR, "Error code %d sending data on connection thread_handle %d (SOCKET = %llu).",
							WSAGetLastError(), sendConnectionHandle, sendConnection.socket);

						// A connection waiting to be closed will never manage to flush, drop the data so it can close.
						if (sendConnection.state == win32_active_connection::STATE::SERVER_CLOSED)
						{
							sendConnection.send_buffer.discard(sendConnection.send_buffer.item_count);
						}
					}
				}
			}
			InterlockedExchange8(&sendConnection.sending_data, 0);
		}
	}

	return 0;
}

// END SEND THREAD
