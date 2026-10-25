# MATCH SLOTS GAME SERVER SUBCOMPONENT FOLDER

This folder contains the implementation of the Match Slots subcomponent of the Game Server.

Each Match Slot is a statically-allocated resource with a state cycle allowing the use and re-use of the same memory footprint to run any number of matches in a row so long as the server is alive.

The intention is for each slot to effectively be its own kind of sub-process, perhaps running in its own thread, and controlled from the game server at a high level while the match slot code itself handles most behaviors (including game messages received from clients, and broadcasting game messages from the server).

## Code org

The main lifecycle and common helper function symbols are declared in the `match_slots.h` header file.

Some states that are particularly complex / feature-rich have their own header with extra functions, but regardless of this the main lifecycle calls (going from one state to another) remain in the main header.

Similarly, those complex states also feature their own implementation file, aggregated together in the _main.cpp file of the subcomponent. The implementation of ALL functions that concern that state are inside the state's implementation file, if it exists.

The Match Slots is an extension subcomponent of the game server and effectively adds to its functionality directly, meaning most functions take the game_server structure itself as their first parameter directly.

## States

Slots must first be initialized. From there they enter the WAITING state.

WAITING is followed by IN_LOBBY when the lobby is opened to Game Client connections. In the lobby, the game's rules and parameters are set before starting.

IN_LOBBY is followed by IN_PLAY when the match actually begins and the actual match code & world simulation begins, handling the flagship Lockstep network synchronization mechanism, and AI player logic on the side (which send commands as human players do).

PLAY_ENDED is reached whenever the match is ended for any reason. In that state, the slot maintains the full match state but does not continue ticking it, leaving time for various other systems to react to the match's ending to gather analytics and such (@TODO).

AWAITING_CLEANUP is the last state in the lifecycle of the slot, during which its entire contents are considered garbage by the broader game server systems. It can be Reset to return to the WAITING state, where the cycle can begin again.
