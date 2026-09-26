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

The provided Send function takes in a game message (assumed to be smaller than the maximum Websocket frame size), frames it as required by Websocket and sends it.

Reception works through a "buffers split" strategy that relies on the fact we can immediately get rid and react to non-game messages like pings, closes...
The reception buffer stays aware of not only how many bytes are waiting but also how many have been "queued" / processed from their websocket frame.
The receive function on the client can then just reconstruct the processed websocket frame at the beginning of the buffer and obtain its payload location.
The consume function can do the same thing to know how many bytes to consume.

It could still be a nice improvement to turn the buffer into a ring buffer so we don't have to shift the bytes left on consumption.

Websocket clients are pinged regularly as a heartbeat / keep-alive mechanism, based on a ping clock or how long since they sent something to us.
