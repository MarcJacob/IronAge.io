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

### Http Server

The server has a secondary http component which is used to server browser clients with the game client files.

The serveable files are preloaded in memory, and client browser can ask for them by name. It is not a general-purpose serving algorithm, it limits itself to what
is pre-configured for speed, simplicity and security (since it's not possible to ever get served a file that wasn't intended to be served).

## Intention

Most of the game server code should end up existing within this folder. The Game Server will handle the bulk of the work on a platform-independent level, such as:
- Creating and running background threads.
- Manage connection sockets (both for matches and http / websocket).
- Manage its own memory from what the platform gave it on initialization (policy will always be to spawn more game servers if more matches must be simulated).
- The vast majority of logging.

Currently the network architecture is envisioned to be a web frontend connected to a Game Server backend written in C++ for both the main match simulation and the http file serving.