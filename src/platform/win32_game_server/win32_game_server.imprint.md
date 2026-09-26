# WIN32 GAME SERVER PLATFORM LAYER SOURCES

Full Win32 implementation, entry point and compilation point for the Win32 Game Server.

The bulk of the code lives in _main.cpp, and it is also where everything else gets unity-compiled.

However, with complexification of the platform code to handle network activity there's now multiple files which justified putting it in its own folder.

Time will tell if we'll eventually move some code to some sort of "win32 common" folder for re-use by other Win32 platform programs.

## Win32 services for the game server

The Win32 platform allocates a generous amount of memory for the server and does not yet support giving more if needed at runtime.

It provides standard & error output through a Windows console which is also the only window that is opened (therefore other means of visual feedback are not supported / stubs).

Uptime is measured using Win32's QueryPerformanceCounter and QueryPerformanceFrequency to provide millisecond-accurate integer time measurements as required by the server.

Networking is provided by a WinSock2-based implementation.

## Net component (win32_game_server_net.cpp)

Three background threads (listen, reception, send) work on a fixed table of connections. A connection handle is its index in the table, and handles are reused.
Each connection has its own reception and send ring buffers (single producer, single consumer). The "*net_*" functions the game server calls only touch those buffers, never sockets.

A connection's *state* field is the ownership handoff between threads: EMPTY -> CONNECTED (listen thread) -> OPEN (game server acknowledges it when querying new connections)
  -> PEER_CLOSED / SERVER_CLOSED -> CLOSED (reception thread, once no data is left to read or send) -> ENDED (game server acknowledges it when querying closed connections) -> EMPTY.
  
Known limit: a peer that stays connected but stops reading is never closed once the server asked to close it. Eventually the closing status will be accompanied by a time-to-close value.
