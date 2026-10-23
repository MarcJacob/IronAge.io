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
index. Add an entry here when a unit starts, flag it `[DONE]` at the end of the line
when it finishes.

- `work_units/platform_list_resource_files_2026-09-20.md` [DONE]
- `work_units/web_server_file_auto_reload_2026-09-20.md`
- `work_units/server_dev_mode_2026-10-03.md`
- `work_units/build_system_revamp_2026-10-03.md` [DONE]
- `work_units/dev_iteration_loop_2026-10-03.md`
- `work_units/string_pass_2026-10-03.md` [DONE]
- `work_units/blit_canvas_2026-09-26.md` [DONE]
- `work_units/village_influence_2026-10-03.md` [DONE]
- `work_units/game_server_match_lifecycle_2026-10-03.md` [DONE]
- `work_units/lobby_system_2026-09-27.md`
- `work_units/start_location_choice_2026-10-03.md` [DONE]
- `work_units/road_construction_2026-10-03.md` [DONE]
- `work_units/straight_line_movement_2026-10-15.md`
- `work_units/road_system_2026-10-21.md`
- `work_units/command_wiring_simplification_2026-10-18.md` [DONE]
- `work_units/world_sim_reorganization_2026-10-18.md` [DONE]
- `work_units/world_sim_simplification_2026-10-19.md`
- `work_units/basic_world_economy_2026-10-18.md` [DONE]
- `work_units/trade_caravans_2026-10-22.md` [DONE]
- `work_units/economy_revamp_2026-10-22.md` [DONE]
- `work_units/alpha_troops_recruitment_2026-10-18.md`
- `work_units/alpha_war_extensions_2026-10-18.md`
- `work_units/alpha_diplomacy_chat_2026-10-18.md`
- `work_units/alpha_vassalization_2026-10-18.md`
- `work_units/alpha_victory_score_2026-10-18.md`
- `work_units/placement_pick_validity_2026-10-13.md` [DONE]
- `work_units/public_hosting_2026-10-15.md` [DONE]
- `work_units/basic_army_battle_system_2026-10-15.md` [DONE]
- `work_units/map_gen_2026-10-22.md` [DONE]
- `work_units/match_ai_2026-10-22.md` [DONE]
- `work_units/entity_player_colors_2026-10-23.md` [DONE]
- `work_units/army_supplies_2026-10-23.md`
- `work_units/web_ui_static_modules_2026-10-23.md`

Finished units are moved to `management/archive/` after a time.

## Development Phases

High-level sequencing, not a scope commitment. Mark `[WIP]`/`[DONE]`; keep finished
work to a line or two.

1. [DONE] **Architecture Skeleton** - thin end-to-end slice, no real game rules yet. Expected
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

2. [DONE] **Dev infrastructure & cleanup** - overhauls to speed up iteration before main game
   development. Backlog files in `management/backlog/` hold the details.
   - [DONE] Server resource discovery: platform call listing resource files, web server
     loading serveable files from the discovered list.
   Sequence:
   - [DONE] String pass (`string_pass`): proper string toolset (views, arena-backed
     utilities, string builder, static arena) used across the code; C strings only for
     constant strings and direct platform calls that require them.
     - Platform resource paths not null-terminated.
   - Server dev mode: admin-authenticated dev messages (first action: reload resources).
     (`server_dev_mode`) Deferred to phase 4: current setup is enough until the game matures.
   - Live resource reload at runtime (in-flight responses, re-listing, auto-reload on
     change; TODO in web server). (`web_server_file_auto_reload`)
     - [DONE] Crude version: resources rediscovered / reloaded on the first tick after a
       server DLL hot reload.
   - [DONE] Build system revamp: off CMake, Game Common as a static library, server as a
     hot-reloadable dynamic library. (`build_system_revamp`)
   - Dev iteration loop: one-step build / deploy / hot-reload / resource reload.
     (`dev_iteration_loop`) Deferred to phase 4, same reason.

3. **Alpha gameplay & framework** - gameplay additions forming a coherent "Mini" version
   of the game (design doc's Alpha section, to be written), plus game-framework work.
   Ends with the last Alpha version.
   - [DONE] Preparation, before any feature work below:
     - [DONE] Simplify command wiring: fewer places to touch to go from a game feature to a
       command to a client modal control scheme. (`command_wiring_simplification`)
     - [DONE] Reorganize world simulation code: one implementation file per system / category, helper
       headers per theme. (`world_sim_reorganization`)
   - Alpha features, in this order, after the preparation: `basic_world_economy`
     (time system, wealth / treasury, buildings, trade), `alpha_troops_recruitment` (war economy),
     `alpha_war_extensions`, `alpha_diplomacy_chat`, `alpha_vassalization`,
     `alpha_victory_score` (plus `straight_line_movement`).
   - Formalize the feature-branch workflow.
   - Later, once an example of each kind of feature exists end-to-end: formalize how
     mechanics get added, and write artisan pipelines from those working code shapes.

4. **Infrastructure** - tidy server hot reload / resources design (room for larger files:
   music, advanced graphics; includes deferred dev mode and iteration loop), proper
   logging system, Linux game server platform layer (cheaper, more powerful hosting).

5. **Beta** - starts marketing and community building; more mechanics (longer games, more
   complexity per the design doc's Beta section) and user-facing features, incl. proper
   user accounts.

## Backlog

Tasks not currently part of the plan that need to be added to it at some point. Each has its
own file under `management/backlog/<name>_<input-date>.md`: the item itself, a description,
why it's there, and the date it was first raised (so old items can be prioritized or
discarded). Add new items there directly - see the folder for the current list.
