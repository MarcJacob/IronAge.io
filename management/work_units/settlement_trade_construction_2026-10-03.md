# Settlement trade and construction

Started: 2026-10-03 (as `settlement_econ_tick`, renamed 2026-10-12)

## Goal

Next settlement economy mechanics on top of the existing population growth / land output:
trade and construction (e.g. infrastructure such as roads, using tile flags).

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
3. [WIP] Wire movement orders to it. First part DONE, user-tested: entities follow paths.
   - Entity movement = `destination` + `move_target` (next tile) + `path_handle` (always `INVALID_PATH_HANDLE` for now). Orders write only `destination`.
   - Minimal `world_path_store` (opaque ptr in `match_world_state`, fixed 256 KiB arena, last path only); `world_request_path` is `void`, `path_result.count == 0` = no path.
   - Path recalculated at every tile (to be removed by the cache, task 4).
   - No-path behavior left as is (stays put, retries every tick): unreachable destination will be handled later as a cross-system concern, not in movement.
4. Moved to backlog: `backlog/path_store_cache_2026-10-15.md` (store, handles, cache). Straight-line movement: `backlog/straight_line_movement_2026-10-15.md`.
5. [ ] Construction system (settlement-side) + road as first on-map type; input command only dispatches.
   - `world_tile_move_cost` already reads the unused `is_road` bit; nothing sets it yet.
6. [DONE] Client preview of path (selected entity to hovered tile), via `game_client_query_path`, 256 KiB `path_scratch` arena. Road preview comes with task 5.

## Decisions (pathing)

- Path = waypoints, each leg a run in one of 8 directions, start tile excluded; always returns a result (possibly empty / partial).
- Functions about the world or its terrain get a `world_` prefix.
- `world_request_path` store is for sim code only; the stateless tool is for anyone.

## Known limitations / cleanups

- Diagonals can cut between two blocked tiles.
- Heuristic assumes min tile cost 1 (wide search on plains); improve if previews get slow.
- Client ignores `partial`; preview buffer capped at 64 waypoints; single land mover type.
- `PROXIMITY_WEIGHT_*` shared by economy and pathing: rename neutrally.
- Off-by-one: move-target validity check in match_commands.cpp uses `>` instead of `>=`.
- Nothing committed yet (user's call).

## Depends on

`village_influence_2026-10-03.md`.

## Progress

Pathing tool, client preview and movement wiring done. Next: task 5 (construction system + roads), design first.
