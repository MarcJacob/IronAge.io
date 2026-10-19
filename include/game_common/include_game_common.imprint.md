# GAME COMMON PUBLIC SOURCES

This folder contains the public-facing symbols for the Game Common logic, which mostly includes the ability to simulate game matches using input commands,
and the network-facing Game Message structures.

*game_data_object.h* defines Game Data Objects (GDOs): a way to describe functional game data such as messages and input commands, so it can be interacted with across language / network boundaries.

*/match/* contains the headers for starting, simulating and inspecting a match (input commands, start parameters, AI logic, and the */world/* subfolder with world state, terrain, entities and pathing). See its imprint.

## Game Messages

Games messages are a network-oriented protocol for communicating specifically between a client and server in the IronAge "ecosystem". They acknowledge two roles:
- Server, which is where authoritative game state is located.
- Client, which is a source of inputs and expected to know the correct match state by receicing whatever inputs were applied over each match tick from the server.

Beyond matches themselves, game messages are used for all out-of-game communications as well:
- Joining / leaving a match.
- Making edits to a lobby.
- Inter-player communications.
- ...

They are an abstraction over how the client works specifically and how the messages are sources / sent. This way, it is possible for clients to exist in any number of forms:
- Standard network clients using various connection protocols (Raw TCP, Websocket, ... *raw http ? I shudder to imagine*).
- Local client, through a "Host mode" on the client that can act both as a server and a working client in the same app.
- Machine-local clients connected through loop-back raw sockets spawned by the server for testing or running more advanced AIs.
- ... 

### Structure

Game messages are implemented in three parts:
- An entry in the GAME_MESSAGE_TYPE enumeration.
- A payload structure.
- Commonly, inlined layout-reader functions inside payloads with dynamically-sized elements.

### Game Server <-> Game Client Protocol

Arrival:
- Client connects and gets promoted to Game Client status on the server through a supported mechanism.
- Server -> Client: "Lobby Joined" message, on joining and whenever the lobby changes.
	- Contains the lobby state: countdown before the match starts, player count, match slot, start params.
- Server -> Client: "Match Started" message, when the lobby countdown ends.
	- Contains enough info to deterministically constitute the starting match state on client, what player the client is supposed to control, and the (indicative) placement countdown.
	- No reply from the client.

Placement phase (from "Match Started" until the first "Server Tick"; there is no phase message):
- Server -> Client: "Server Placement Pick" messages, for every player (pre-placed at random, then on every valid change).
- Client -> Server: "Client Placement Pick" message, to change its own start location. Invalid picks and picks outside the phase are discarded silently.
- The client stays in placement until the first "Server Tick", whatever the countdown says.

On each Server tick until MATCH END:
- IF Client is *late* = Server -> Client: "Server Tick Bundle" messages.
	- Only sent in "catchup" moments when the client is joining late, reconnecting, or is too far behind.
	- Bundles contain multiple ticks worth of input command sequences for faster catchup.
	- The client must apply the commands to their local match state and preferably stop allowing the emission of input commands to the server.
- IF Client is *current* = Server -> Client: "Server Tick" message.
	- Contains a full Tick Commands structure and the tick it is supposed to apply to. **The client must not have already simulated the tick locally**.
	- Is trusted to contain the exhaustive sequence of ticks applied to the match on that tick on the server.
- IF Client is *current* = Client -> Server: "Client Tick" message.
	- Contains a sequence of input commands and the visible match tick on the client at the moment of emission.
	- The server is free to apply or discard any or all of those inputs at their discretion.

- On server-side match ending (due to game rules or server ending the match manually), or client departure, MATCH END.

Client Departure:
- Server -> Client: "Kick" message.
	- "Politely" tells the Client the server has unilaterally ended the connection. May contain information about the reason why (and no-return timer).
- Client -> Server: "Disconnect" message.
    - "Politely" tells the Server the client has unilaterally ended the connection, and did so deliberately.
- Server detects Client is for any reason unable to be a source of inputs for too long = "Kick" message, if needed.
- On MATCH END = Server -> Client: "Match Ended" message.
	- Contains the winner player id (or none). **The connection is NOT dropped**: the client resets its local match state and the server re-attaches it to a lobby ("Lobby Joined" message follows).
