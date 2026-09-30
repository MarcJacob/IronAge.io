# GAME COMMON PUBLIC SOURCES

This folder contains the public-facing symbols for the Game Common logic, which mostly includes the ability to simulate game matches using input commands,
and the network-facing Game Message structures.

## Match System

IronAge matches have an approach to simulation intended to work with a Lockstep mechanism: a *sequence* of *input commands* are applied over a match's world state,
and affect it alongside the standard tick / time integration to **deterministically** output a new world state. This way, supporting networked multiplayer
only requires broadcasting inputs from the server and for each client to send theirs as fast as possible.

Matches identify players by a simple ID. It is up to the host app to match this ID with some sort of control scheme (Local player, network player, AI...) coherently.

### Commands

The Input Command system is based on identifiable *Command Types* each associated with a *payload size*, *payload structure* and two fundamental functions:
- Apply, which takes in the appropriate emitting player ID and payload structure and applies it over a specified match's state.
- Validity Check, which takes in the same parameters and estimates whether the command is valid (mostly that the parameters are coherent and that the emitting player is
	actually allowed to do that).

Outside their final applications commands usually exist in simple byte buffers along with a command count, to be parsed read command-by-command.

Each input command type is structured this way:
- Entry in the COMMAND_TYPES enumeration.
- Payload structure (with rare cases of re-use of the same structure, although inheritance is recommended).
- Implementation functions, some essential like Apply and Validity Check, others optional (compression / decompression ?)

Then the functions must be used in the appropriate match host and match tick code (early tick function when all input sequences are processed).

Guidelines:
- They must be entirely implemented within the Game Common codebase.
- Payload structures must have consistent padding behavior, with no padding at all preferably (for compatibility across machines).
- Payload structures must be as small as possible (striking a good balance with parsing work).
- Validity Check must adopt a "return false as soon as possible" mindset.
- Apply must do its work as quickly as possible as each command is run sequentially pre-tick.
	- Do not mix up what should be in the match's tick work and what should be in the Apply function !

Later on, we may want to make the registration of commands more convenient and centralized, in the sense that we should be able to see every element of a command
together in one place. This could be done with a macro whose role is to output a command to some static buffer any program could choose to place in their static memory where they wish.
Then a command's *code* could be related to its index in that collection instead of its command type enumeration value (which could even be replaced with a string name).

Another advantage of a more data-oriented approach like that would be that the match code could loop over all commands abstractly, such that match code wouldn't need to be touched
to implement the effects of a command.

#### Structuring for input into match tick

Commands follow a structure leading to the the root "full tick commands" structure that is passed to a match for ticking:
- Command Header + Payload[] = "Basic command" with only its type and payload.
- Player ID + Command Count + Commands Buffer[] = "Command Sequence" linking a player to the sequence of commands they are sending.
- Total byte size + Sequences Count + Sequences[] = "Tick Commands" putting together all sequences to run for a tick as a single memory block.

Sequences or full Tick Commands structures can be built using corresponding builder structures for an easier time feeding into such structures from generalized input logic.

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
- Server -> Client: "Join Match" message.
	- Contains enough info to deterministically constitute (current or starting) match state on client, and what player the client is supposed to control.
- Client -> Server: "Match Joined" message.
	- Confirms the client has the match state ready.

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
	- Similar to Kick message. **Currently the client can assume this message acts as a Kick and that the connection will be dropped unilaterally**.
