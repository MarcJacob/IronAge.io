# Road construction

Started: 2026-10-03 (as `settlement_econ_tick`, then `settlement_trade_construction`; renamed 2026-10-18)

## Goal

Pathfinding and road building. Ended up serving as the preliminary design of the construction
system (builder entity, build type table, territory-limited paths); trade and buildings are
separate work units, planned from the Alpha design doc.

## Done (from the former econ tick scope)

- Population growth toward a target = land output + local wealth; land output from owned
  tiles weighted by influence and contestation (see `village_influence_2026-10-03.md`).
  Good enough for now; capacity = `pop + 10 + pop / 2`.

## Open design

- Trade: inputs, caravans, local wealth, trade attractivity; see design_doc.md.
- Construction: what can be built, costs, effect on tile cost / output.
- Tick cadence relative to the sim tick.

## Decisions

- Road experiment (commits 1830051, reverted in 5e776f3) is a throwaway; starting fresh.
- Road on a tile = low travel cost regardless of terrain (kept idea).
- Not instant, and not "from nearest road to target" (dropped).
- Pathfinding is its own general system, not road-specific (experiment's was too complex and road-focused).

- Pathfinding, two layers in game common (client and server both use it):
  - Low: synchronous path request, caller supplies the memory, returns a path. Client calls it directly for previews (movement, road construction).
  - High: world-wide path cache for re-using results.
  - Full tile-grid resolution.
- Construction: instant for now, but logic lives in a settlement construction system, never in the input command (so it can become timed later).
- Road target: player picks target, path auto-computed from the settlement; maybe separate start/end later.
- Generalized construction start: on-map elements (roads first) and in-settlement elements; may diverge later.

## Tasks

1. [DONE] Read-only survey.
2. [DONE] Pathfinding low layer: stateless `world_path_find` ([pathing.h](include/game_common/match/pathing.h), [world_pathing.cpp](src/game_common/world_sim/world_pathing.cpp)); A* on 8 directions, 5/7 chamfer costs, scratch arena size = search budget, deterministic tie-break.
3. [DONE] Wire movement orders to it: entities follow paths. Remainder (straight-line movement, cost-based speed, direct-movement order) moved to `straight_line_movement_2026-10-15.md`.
   - Entity movement = `destination` + `move_target` (next tile) + `path_handle` (always `INVALID_PATH_HANDLE` for now). Orders write only `destination`.
   - Minimal `world_path_store` (opaque ptr in `match_world_state`, fixed 256 KiB arena, last path only); `world_request_path` is `void`, `path_result.count == 0` = no path.
   - Path recalculated at every tile (to be removed by the cache, task 4).
   - No-path behavior left as is (stays put, retries every tick): unreachable destination will be handled later as a cross-system concern, not in movement.
4. Moved to backlog: `backlog/path_store_cache_2026-10-15.md` (store, handles, cache). Straight-line movement: `backlog/straight_line_movement_2026-10-15.md`.
5. [DONE] (user-verified) Construction system (settlement-side) + road as first on-map type; input command only dispatches.
   - `world_tile_move_cost` already reads the unused `is_road` bit; nothing sets it yet.
   - Steps (survey done; territory = per-tile `owners[]` slot index in region):
     - 5a-5d [DONE] Server side (peer): construction system + `BUILD_ROAD` command (enum 5, 12 bytes, wired in all 3 switches). Construction system tested by a temporary trigger (removed); the command itself is built but untested until a client sends it. Wire change: client / server builds must match.
       - Built as a `BUILDER` micro entity (origin entity, build type, ticks worked, 16-byte `action_data`) with a `BUILD_TYPE` definitions table; road needs a SETTLEMENT origin.
       - `world_system_construction.cpp` runs first in `world_state_tick`: re-paths every tick, sets one road tile per tick inside the origin's territory; ends on destination reached or no progress.
       - Territory limit = `path_params::within_settlement_territory`, checked in `world_tile_move_cost` (replaced the planned result filtering; so no public prefix filter).
       - `world_tile_move_cost` is the shared cost (road = 1).
     - 5e-5h [DONE] Front (peer), pending user's hand check of the full flow:
       - Modal system in front/src/modes/ (`Mode` class in core.ts, mode stack; input to top mode only, frame updates and net messages to the whole stack): LobbyBase, GameBase, SpawnPick, MainPlay, SettlementSelected, RoadBuilding. UI files moved to front/src/ui/.
       - `BUILD_ROAD` client input path; `game_client_query_command_valid` (+ wasm export) reuses the server's `match_command_validity_check`.
       - Roads rendered via `RENDER_CONFIG.show_infrastructure`; preview limited to the territory, red when invalid, 256 waypoints.
       - Road mode: selected own settlement, key `1` or button; `1` again or Escape leaves; stays active after an order.
   - Notes: tiles not in snapshot / hash (roads arrive via commands); influence proximity ignores roads (leave); no dirty-rect system, territory layer redrawn every frame; terrain layer drawn once.
6. [DONE] Client preview of path (selected entity to hovered tile), via `game_client_query_path`, 256 KiB `path_scratch` arena. Road preview comes with task 5.

## Decisions (roads, task 5)

- Road = tile modifier only: travel cost 1 regardless of terrain.
- No build cost for now (iterating on UX flow and building system); cost comes with a fleshed-out economy.
- UX: owner selects settlement, clicks / hotkey enters road-building mode; player picks destination; default origin = the settlement, changeable by clicking an already-built road tile or the settlement again.
- Logic in settlement construction system; input command only dispatches; instant for now.
- Client previews the road path via `game_client_query_path`.
- Player may target any tile; construction stops at the edge of the settlement's territory. Implemented as a path search restricted to the territory (`within_settlement_territory`), superseding the earlier result-filtering idea. Out-of-territory origin refused.
- Later: sub-query bridging road segments separated by another territory; roads through any territory the same player owns.
- Command payload: settlement guid + origin + destination; validated as owned by the sender. Empty / refused commands dropped silently.
- Origin rule enforced on purpose (avoids unconnected roads): the settlement's own tile, or any road tile inside its territory, whoever built it. Not any territory tile.
- Building a road flags the tile for proximity recalc, so roads affect proximity-based systems. Influence proximity must use the same tile travel cost as pathfinding (one shared cost function accounting for everything); today it uses `TILE_TERRAIN_TRAVEL_COST` directly.
- Work on this goes through peer programmers (user's choice), not builders.
- Long roads: build the truncated part; raise client preview cap 64 -> 256 waypoints.
- Road visuals: translucent gray, whole tile, drawn on top of everything incl. borders. Later: in-tile shape by neighbouring roads.
- Client input: "entity action system" = modal input modes activated per entity type; road building is the first (settlements), as proof of concept. Several roads can be built without leaving the mode.
- Client wants to query whether an input command is currently valid (reuse the command validity function on the client) and simplify client input command code. Design needed before 5e-5h.
- Any entity uses any road.
- Road removal later (idea: maintenance cost, least-used roads vanish first).
- Command carries origin + destination (extensible); the server computes the path.

## Decisions (pathing)

- Path = waypoints, each leg a run in one of 8 directions, start tile excluded; always returns a result (possibly empty / partial).
- Functions about the world or its terrain get a `world_` prefix.
- `world_request_path` store is for sim code only; the stateless tool is for anyone.

## Known limitations / cleanups

- Proximity recalc now uses `world_tile_move_cost` (fixed). Unverified: road build flags tiles for proximity recalc. Possibly unused `tileTerrain` at world_system_economy.cpp:296.
- `BUILD_ROAD` origin == destination must be invalid: user adds it to the validity check himself.
- Worker slots use `MICRO_ENTITIES_PER_TYPE_PER_REGION` (32/region); make dynamic eventually. `match_get_required_mem` does not count builders (32 MiB headroom covers it for now).
- Nothing of task 5 committed yet (user's call).
- Diagonals can cut between two blocked tiles.
- Heuristic assumes min tile cost 1 (wide search on plains); improve if previews get slow.
- Client ignores `partial`; single land mover type.
- Road mode hint text hardcodes "[1]"; settlement bar only has "Build Road" (spawn army / found settlement still in the old panel).
- Cleanup: stale deployed files in web_root/src/ (game_ui.js, lobby_ui.js, placement.js, maybe debug.js / websocket_tests.js); dead front/src/placement.ts.
- `PROXIMITY_WEIGHT_*` shared by economy and pathing: rename neutrally.
- Off-by-one: move-target validity check in match_commands.cpp uses `>` instead of `>=`.
- Nothing committed yet (user's call).

## Depends on

`village_influence_2026-10-03.md`.

## Progress

Pathing tool, client preview and movement wiring done. Task 5 (roads) built end to end: server 5a-5d, front 5e-5h (modal system, validity query). Complete. Trade / buildings continue in new work units.
