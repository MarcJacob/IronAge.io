# IRONAGE.IO SERVER - DEVELOPMENT PLAN (FIRST VERSION, AI GENERATED BECAUSE I KNOW LITTLE ABOUT THE UWEBSOCKET LIB OR WEB FRONT-END DEV).

Note: This was AI generated and it looks like the AI kind of overloaded the technical complexity upfront before getting to the fun stuff...
I will redesign this plan to be a little more specific and try to get to a full Front end input / Server feature / Front end output loop as quickly
as possible.

High-level development plan and stated technical architecture for the IronAge.io Game Server.
See ../design_doc.md for the game design itself. This document is about building it.

## Technical Architecture

### Split: Platform vs. Game Server

The codebase is split into two layers:

- **Platform layer** (`src/<platform>/`, e.g. `src/win32/`): OS-specific glue.
  Responsible for process startup, allocating a single large memory block up front,
  providing the `game_server_platform` struct of callback function pointers (logging,
  file access, networking, threading, and eventually hot-reload of the server as a DLL),
  and driving the main tick loop.
- **Game Server core** (`src/server/`, exposed via `include/game_server/`): fully
  platform-independent. Receives a `game_server_platform&` and a pre-allocated memory
  block on `game_server_init`, and from then on must never request more memory than it
  was initially given - if more capacity is needed, the policy is to spawn additional
  Game Server instances rather than grow one. Ticked externally via `game_server_tick`.

The Platform and Game Server layers are expected to eventually be split into separate
compilation units, with the Game Server built as a hot-reloadable dynamic library.

### Responsibilities of the Game Server core

- Manage player connections registering for a match before it starts.
- Manage player connections for players currently in a match.
- Tie together players and match instances.
- Simulate up to N match instances in parallel, N bound by allocated resources.
- Own and sub-allocate all of its memory from the block given at init.
- Manage background threads and connection sockets itself (platform only exposes the
  primitives).
- Own the majority of logging.
- Send end-of-match reports to whatever needs them (initially: nothing external; later:
  a master server / persistence layer).

### Networking

Planned on top of uWebSockets. In an early development phase, a web frontend connects
directly to a single Game Server instance for development/testing purposes. Later, a
separate master server will let Game Server instances and players discover one another,
and will own a persistent database for long-term, cross-match score keeping. Direct
frontend-to-server connection is expected to remain available as a
development/self-hosting mode even after the master server exists.

### Shared conventions

- Common types and macros live under `include/core/` (`std_types.h`, `assert.h`) and are
  pulled in via `include/core.h`.
- Game-Server-wide symbols live under `include/game_server/`: `game_server.h` for
  symbols used across the game server's own code, `game_server_platform.h` strictly for
  the Platform <-> Game Server interface.

## Development Phases

This is a high-level sequencing, not a commitment to exact scope per phase. Consult this
plan (and update it) as work is assigned or completed. Mark items `[WIP]` while in
progress and `[DONE]` once complete, adding sub-items as tasks are better understood and
broken down.

1. **Scaffolding** [DONE] - repo/build setup, platform entry point, up-front memory
   allocation, minimal platform log callbacks, initial `game_server_init` /
   `game_server_tick` stubs.
2. **Core server runtime** - real memory sub-allocation strategy inside the Game
   Server, background thread management, a real tick/timing loop, structured logging
   through the platform.
3. **Networking** - integrate uWebSockets, accept player connections, basic
   connect/disconnect/message handling, no game logic yet.
4. **Match lifecycle** - registration/lobby handling, starting a match instance,
   running multiple match instances in parallel within one Game Server, end-of-match
   reporting hook.
5. **World & settlements simulation** - texelized terrain, settlement founding/growth,
   land exploitation & influence radius, local wealth/treasury.
6. **Trade** - caravan/fleet generation, routing over roads/sea lanes, target
   selection, wealth generation on stops.
7. **War & armies** - troop types, army movement/pathfinding, battles,
   conquer/raid/sack/raze, upkeep & desertion.
8. **Diplomacy** - base/personal relationships, vassal chains, derived diplomatic
   statuses.
9. **Victory & scoring** - the four victory conditions, runner-ups, secondary victors,
   overall score.
10. **Persistence & master server** - matchmaking/discovery service, persistent
    database for cross-match scoring, moving off direct frontend-to-server dev mode as
    the default.
11. **Ops & hardening** - hot-reloading the Game Server as a dynamic library, simple
    runtime graphics/telemetry for managing a running server, resilience/perf passes.

Phases are sequential in dependency terms (later phases build on earlier ones) but not
strictly in calendar terms - e.g. diplomacy and war can be developed in parallel once
the world simulation exists.
