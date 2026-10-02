# IRONAGE.IO - DEVELOPMENT PLAN

Development plan and technical architecture for IronAge.io: Game Server, web Game
Client, and the shared GameCommon simulation core. See ../design_doc.md for the game
design itself.

Approach: build a thin, working, end-to-end slice through every component first (an
"architecture skeleton") before building any one component out fully, so there's a real
cross-component testing loop from early on.

## Active Work Units

Detailed design / progress for in-flight, coherent task groups lives in
`management/work_units/<name>_<start-date>.md`, keeping this file a high-level
index. Update the entry here when a unit starts / finishes.

- [DONE] TypeScript migration - `work_units/typescript_migration_2026-09-27.md`
- [DONE] Client input & camera prep - `work_units/client_input_2026-09-27.md`
- [DONE] Game Client / Platform split - `work_units/game_client_platform_split_2026-09-29.md`
- [DONE] World entities & settlement rendering - `work_units/world_entities_2026-10-01.md`
- [DONE] Deterministic random generation - `work_units/random_generation_2026-10-02.md`
- [WIP] UI & first commands - `work_units/ui_first_commands_2026-10-02.md`

## Development Phases

High-level sequencing, not a scope commitment. Mark `[WIP]`/`[DONE]`/`[NEXT]`; keep finished
work to a line or two.

1. **Architecture Skeleton** - thin end-to-end slice, no real game rules yet. Expected
   result: browser shows a visible element changing in sync, driven by a lockstep-ticked
   GameCommon instance, relayed through the Game Server, rendered via the current JS
   canvas (with the blit canvas deferred).
   - [DONE] Project setup: CMake targets for the native win32 server and the wasm32 client
     (both including GameCommon), JS harness loading the `.wasm`.
   - [DONE] GameCommon skeleton: match create / tick(input) / dump, arena allocator, trivial
     sim (one entity moving to a target), deterministic snapshot. Native and wasm snapshots
     are byte-diffed by `determinism_test.html`.
   - [DONE] Server platform + headless server: 4 GiB up-front block, match slots with a
     lifecycle and a memory arena each, ongoing matches ticked on a fixed schedule from the
     platform's ms timestamp.
   - [DONE] Client platform, standalone: wasm exports, JS fixed-rate loop, JS canvas with
     click-to-set-target.
   - [DONE] Win32 networking: listen / reception / send threads over ring buffers and a
     connection table behind a pull-based platform interface (all-or-nothing sends, clean
     shutdown). Platform file access under `GAME_SERVER_RESOURCES_DIR`.
   - [DONE] Logging: platform `log` / `logf` with `LOG_TYPE`, colored win32 end points,
     `server.log` / `server.logf` with component prefixes.
   - [DONE] Web server: preloaded static files over HTTP (structured request parse, GET /
     HEAD, error responses, timeouts), sized string helpers (`include/core/string.h`).
   - [DONE] Client ownership on the game server: clients table (UNKNOWN -> NON_GAME_CLIENT ->
     GAME_CLIENT), disconnect handlers, http -> websocket upgrade handing over leftover bytes
     without a copy, send / peek / consume message functions.
   - [DONE] WebSocket: SHA-1 + base64 (`include/core/math.h`), handshake with `Origin` check,
     frame codec, ping keepalive, framed sending. Browser tests (`src/websocket_tests.js`):
     all passing.
   - [DONE] Game protocol v0 over the WebSocket layer, replacing the temporary echo hook.
     Proof achieved: two browser tabs connected to the same server show synchronized state,
     each tab moving its own entity via real click input, round-tripped through the server.
     - Slot 0 hard-routing; primitive lobby (starts once 2 clients are connected, no player
       list / ready-up yet).
     - `match_player` = `{client_handle}` array on `match_slot`, indexed by player index;
       bridges clients (later AI) to player indices. Disconnects detected by polling
       (stale handle) while gathering input, no event handler.
     - Messages: `MATCH_JOINED` (embedded packed `game_match_start_params` + player index,
       sent right on slot attach), `SERVER_TICK` (one tick's packed `match_tick_commands`,
       embedded in the payload), `CLIENT_TICK` (one command, built from the client's last
       queued input and sent back in response to the `SERVER_TICK` that prompted it - no
       separate client clock).
     - Command / message structs are packed (`#pragma pack(push, 1)`) so server and client
       agree on layout byte-for-byte; sizes are unconditional even in debug builds.
     - No input log, no catch-up / bundling, no tick staleness gating yet - see Backlog.

2. **World & settlements simulation** through **11. Ops & hardening** - phases from the
   previous plan version (world/settlements, trade, war, diplomacy, victory & scoring,
   persistence & master server, ops & hardening) still look like roughly the right
   sequence, but haven't been re-specified against the architecture above yet. Revisit
   phase by phase in a future pass.
   - [DONE] 2026-10-01/02: world entity storage, spawning, and rendering for
     settlements/armies/caravans - `work_units/world_entities_2026-10-01.md`.
     Real world-simulation mechanics (growth, caravan trade, combat) remain
     unstarted beyond placeholder test code; a future work unit should pick
     that up.

## Backlog

Tasks not currently part of the plan that need to be added to it at some point. Each has its
own file under `management/backlog/<name>_<input-date>.md`: the item itself, a description,
why it's there, and the date it was first raised (so old items can be prioritized or
discarded). Add new items there directly - see the folder for the current list.
