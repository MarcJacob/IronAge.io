# WEB SERVER (GAME SERVER SUBCOMPONENT) SOURCES

This folder contains the source code of the Web Server subcomponent of the Game Server, in charge of handling general HTTP connections to serve files,
as well as WebSocket Game Client interfacing.

## HTTP Client

Active connection to an HTTP peer. The only provided services are:
	- Serving pre-configured files.
	- Upgrading the connection to using WebSocket.

The serveable files are preloaded in memory, and client browser can ask for them by name. It is not a general-purpose serving algorithm, it limits itself to what
is pre-configured for speed, simplicity and security (since it's not possible to ever get served a file that wasn't intended to be served).

HTTP Clients are promoteable to Websocket clients on demand from the Client, whenever it opens a Websocket.

## WebSocket Game Client

Once a HTTP client gets upgraded to Websocket, we assume it does so for the purpose of being a Game Client through the Websocket protocol.

The Web Server is a connector: it never exposes functions to the game code. On promotion it hands the Game Server Client two buffers (see *game_server_clients.h*), and from then on only moves bytes between them and the network.

Reception: raw network bytes land in a reception buffer, and complete frames are parsed out of it. Control frames (ping, close...) are handled right away.
Game messages (binary frames) are unmasked, validated and copied, payload only, into the inbound processed buffer, where the game code reads them back to back. Frames too large for it are refused.
It could still be a nice improvement to turn the reception buffer into a ring buffer so we don't have to shift the bytes left on removal.

Sending: the game code puts raw game messages back to back in the outbound buffer. Each tick the Web Server frames them one by one into a scratch buffer on the stack and hands them to the platform. Messages the platform refuses stay in the outbound buffer for the next tick.

The arenas in the client block are initialized with their init function at promotion, since they overlay HTTP client data. The reception buffer is aligned with the HTTP request buffer
so any bytes received after the upgrade request are handed over without a copy.
The block only holds data (no function pointers or views into the DLL), so the Game Server code can be hot reloaded.

Websocket clients are pinged regularly as a heartbeat / keep-alive mechanism, based on a ping clock or how long since they sent something to us.
