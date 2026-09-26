# GAME SERVER SOURCES

This folder contains the core of the platform-independent source code for the IronAge.io Game Server.

## What is the Game Server ?

The exact responsibility of the Game Server is to be able to:
- Manage player connections who want to register for a match before it starts.
- Manage player connections who are in a match.
- Tie together players and match instances.
- Simulate up to N match instances in parallel (N = depending on allocated resources).
- Send end-of-match reports to whatever needs them.

At the current stage of the project the Game Server is the sole backend - the front-end directly connects to it for testing purposes.
While this ability might be kept for development purposes, later on there will be a master server linked to a specific web address so Game Server instances,
and prospective players, may meet and discover one another, and so there can be a persistent database for long-term score keeping among players.

## How it works

The game server is platform-independent in code. To start it, an implementation platform must use the game_server_init function, passing it a valid platform structure and enough pre-allocated memory.
From there on the game server will never request more memory than was initially given.

The platform must then track the passage of time, tick the server as much as possible so it may integrate it, and respond to the server's requests for platform resources and capabilities.

### Clients

The Game Server Client system, implemented mostly in game_server_clients, with specific client types managed in their respective associated server components.

Clients start out as Unknown and are given very little time and room to successfully identify themselves into one of the sub-components (Web, native...).
This done, they become "Non-game clients" which only talk directly with their owning sub-component. Eventually they can be upgraded to full Game Clients by
the sub-component, allowing them to start participating in standardized game-related communications with the game server itself (account authentication, lobby joining, gameplay...).

The point of this system is to allow any number of concurrent connection and communications protocols to co-exist and be interpreted as standard game clients by the main game server code.

### Web Server

Server component for handling HTTP & Websocket Clients. Can serve files and upgrade HTTP Clients to Game Clients through the use of the WebSocket protocol.

See web_server folder.

## Intention

Most of the game server code should end up existing within this folder. The Game Server will handle the bulk of the work on a platform-independent level, such as:
- Creating and running background threads.
- Manage connection sockets (both for matches and http / websocket).
- Manage its own memory from what the platform gave it on initialization (policy will always be to spawn more game servers if more matches must be simulated).
- The vast majority of logging.

Eventually we'll want to expand the server's capabilities to also work with native client connections using an app instead of using a web browser, and come up
with some system to authentify connections as administrators with special privileges for remote game server monitoring and control.

By default, any networking activity that isn't specific to a communication protocol should exist on the Game Server level and be done only with identified Game Clients.

Try to keep each sub-component as light as possible, and consider ensuring that they can be ran in parallel with one another (currently we just tick them along with the main game server tick).
