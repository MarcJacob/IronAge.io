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
- Validity Check, which takes in the same parameters and estimates whether the command is valid (mostly that the parameters are coherent and that the emitting player is
	actually allowed to do that).

Outside their final applications commands usually exist in simple byte buffers along with a command count, to be parsed read command-by-command.

#### Creating / editing a command type

Each input command type is structured this way:
- Entry in the COMMAND_TYPES enumeration.
- Payload structure (with rare cases of re-use of the same structure, although inheritance is recommended).
- Implementation functions: Validity Check (checking if it can validly apply over the match from the given player) and Apply (modify world state).
	- For now each of those must be added to the relevant switch statement located in game_common_match_command.cpp, and should probably be implemented there too.

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

## Match elements

Matches have two broad types of elements: the world / world tiles, and entities.

### Entities

Entities collectively are all the dynamic elements of the match's world state, meaning elements whose state can change and whose existence starts and ends at specific points in the match.

Entities are built by composition in the match's *world state* structure using re-used standard property structures where possible. 
Their logic depends both on their properties and their specific type. The match code follows a specific update loop which has the responsibility of applying
deterministic behavior on all entities according to their current state.

Note that the scope of entities can be much larger than "things" in the world, they can also be concepts or events: battles, area effects...
Their only common denominator is that they are separate from the static state of the world, and interactive / relevant to the match simulation.
