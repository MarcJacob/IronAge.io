# MATCH PUBLIC SOURCES

This folder contains header files for interacting with the Match system and surrounding components such as input commands, entities and AI logic.

For the actual simulation / data layout logic, consult <Root>/src/game_common/ code.

## Starting a match

A match can be started simply by calling the *match_start* function and providing it with enough memory and a fully-constituted game_match_start_params structure.
The match will copy the parameters structure at the beginning of the given memory, then initialize its world state accordingly and, if successful, be ready for ticking.

## Match Simulation

IronAge matches have an approach to simulation intended to work with a Lockstep mechanism: a *sequence* of *input commands* is applied over a match's world state,
and affect it alongside the standard tick / time integration to **deterministically** output a new world state. This way, supporting networked multiplayer
only requires broadcasting inputs from the server and for each client to send theirs as fast as possible.

Matches identify players by a simple ID. It is up to the host app to match this ID with some sort of control scheme (Local player, network player, AI...) coherently.

AI player logic does *not* follow those same rules as it also uses input commands to interact with the match, so they do not have to be deterministic and can limit themselves to
running on the server only.

### Commands

The Input Command system is based on identifiable *Command Types* each associated with a *payload size*, *payload structure* and two fundamental functions:
- Apply, which takes in the appropriate emitting player ID and payload structure and applies it over a specified match's state.
- Validity Check, which takes in the same parameters plus an optional feedback structure (*match_command_validity_feedback*) and returns whether the command is valid
	(mostly that the parameters are coherent and that the emitting player is actually allowed to do that).
	Without feedback (the server's path) it returns false at the first error. With feedback it may keep checking and records error flags and detail values
	(what is wrong, what it will cost), which the client shows to the player. A check only sets what it is interested in, the rest stays zero; no error flag means valid.
	Feedback is output only and must never influence the simulation.

Outside their final applications commands usually exist in simple byte buffers along with a command count, to be parsed read command-by-command.

#### Creating / editing a command type

Each input command type is structured this way:
- Entry in the COMMAND_TYPES enumeration.
- Payload structure (with rare cases of re-use of the same structure, although inheritance is recommended).
- Implementation functions: Validity Check (checking if it can validly apply over the match from the given player) and Apply (modify world state).
	- Both take the command header and read their payload structure from it. They are implemented in *match_commands.cpp*.
- Row in the command table (*commands.h*): name, enum value, payload size, Validity Check and Apply, all in enum order.
	- The table lives in the DLL's static data: pointers to rows or functions are not stable across server hot reloads, never store them. Store the command type and look the row up.
- For commands sent from the web client: a Game Data Object definition below the payload structure, and a case in the web client's GDO creation (see *web_game_client* sources).

Then the functions must be used in the appropriate match host and match tick code (early tick function when all input sequences are processed).

Guidelines:
- They must be entirely implemented within the Game Common codebase.
- Payload structures must have consistent padding behavior, with no padding at all preferably (for compatibility across machines).
- Payload structures must be as small as possible (striking a good balance with parsing work).
- Validity Check must adopt a "return false as soon as possible" mindset when no feedback is requested.
- Apply must do its work as quickly as possible as each command is run sequentially pre-tick.
	- Do not mix up what should be in the match's tick work and what should be in the Apply function !

#### Structuring for input into match tick

Commands follow a structure leading to the the root "full tick commands" structure that is passed to a match for ticking:
- Command Header + Payload[] = "Basic command" with only its type and payload.
- Player ID + Command Count + Commands Buffer[] = "Command Sequence" linking a player to the sequence of commands they are sending.
- Total byte size + Sequences Count + Sequences[] = "Tick Commands" putting together all sequences to run for a tick as a single memory block.

Sequences or full Tick Commands structures can be built using corresponding builder structures for an easier time feeding into such structures from generalized input logic.

## Match elements

Matches have two broad types of elements: the world / world tiles, and entities.

Their public state structures are in the */world/* subfolder: *world.h* (world state), *world_terrain.h*, *world_entities.h* and *world_pathing.h*. The terrain and entities are held by pointer in the world state.

### Entities

Entities collectively are all the dynamic elements of the match's world state, meaning elements whose state can change and whose existence starts and ends at specific points in the match.

Entities are built by composition in the match's *world state* structure using re-used standard property structures where possible. 
Their logic depends both on their properties and their specific type. The match code follows a specific update loop which has the responsibility of applying
deterministic behavior on all entities according to their current state.

Note that the scope of entities can be much larger than "things" in the world, they can also be concepts or events: battles, area effects...
Their only common denominator is that they are separate from the static state of the world, and interactive / relevant to the match simulation.
