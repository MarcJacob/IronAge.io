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

Reception is a little more complicated: platform-received bytes are read into a staging buffer for processing either as a standard Websocket message (ping / pong, close signal...)
or as a Game Message, in which case the relevant bytes are moved to a separate buffer which effectively queues up the messages for actual reception through the receive function.

Note: This means that, in total, we have 3 buffers every game message goes through before actually reaching game logic:
- OS Buffer
- Intermediate Buffer
- Messages Buffer

That's a lot of copies. We could get rid of the messages buffer by instead using a ring buffer as intermediate and queuing up messages through pointers into that buffer,
and somehow being able to free items after consumption in any order. Alternatively we guarantee that only game messages ever stay in the reception buffer while other
messages are processed immediately on reception, allowing the bytes to be freed up immediately, but that assumes we receive those non-message bytes cleanly... to be determined.
