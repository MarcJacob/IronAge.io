# Straight-line pathing and movement

Input date: 2026-10-15. Started: 2026-10-18 (moved from backlog; first remainder of `road_construction_2026-10-03.md` task 3).

> 2026-10-29: pathing / movement tasks below are superseded by `pathing_rewrite_2026-10-29.md` (code torn down to a stub skeleton). Kept as history.

## Goal

Movement that is not limited to the 8 grid directions and that actually uses travel cost, plus a direct-movement order.

## Tasks

1. Pathing and movement in true straight lines (not only 8 grid directions); roads included.
2. Movement speed uses tile travel cost (units currently move at a fixed speed).
3. "Direct movement" order for armies: straight line, directness over lowest-cost path.

## Notes

- Related known limitations: diagonals can cut between two blocked tiles; heuristic assumes min tile cost 1.
- Current state: paths are runs in 8 directions (chamfer 5/7 costs); entities follow them tile to tile at fixed speed (`destination` + `move_target` + `path_handle`).
- Origin: `work_units/road_construction_2026-10-03.md`.
- Path store / cache stays in backlog: `backlog/path_store_cache_2026-10-15.md`.

- 2026-10-25: prerequisite of the army rework (`alpha_war_extensions`). Merged with `path_commitment_rewrite_2026-10-22.md` (stored per-entity paths, recompute on change / block; caravan destination selection cost).

## Decisions (2026-10-25)

- All pathing / movement works on fractional positions (tile + sub-tile), consistent speeds. Only static entities (settlements...) stay tile-based.
- Client draws moving entities at their fractional location.
- Effective speed = advertised speed / tile travel cost. Advertised speed = speed on a road (cost 1).
- Stored per-entity paths: recompute on destination change, blocked path, or global path epoch change (roads / territory). No re-path per tile.
- Planned: line-of-sight smoothing after A*, accepted only if straight-line summed cost <= A* subpath cost (keeps roads).
- Preview and sim share per-entity-kind path params and scratch size.

## Findings (investigation, notes in `world_sim/.ai_imprint/pathing_and_movement_state.md`)

- `fractional_loc` is vec2<i8> (+-100, 200 per tile): too coarse for speed / cost (plains = 0.8 unit/tick at speed 4). Needs wider type / finer units.
- Caravan destination selection no longer runs A* (chamfer distance, TODO to restore path-based). `path_commitment_rewrite` / `path_store_cache` notes stale on this.
- `pathing_direction_investigation.md` does not exist.
- Preview differs from sim: params (water), re-path per tile, scratch size.

- Sub-tile position: 16 bits per axis, 1 tile = 1000 sub-units (speed unit = 1/1000 tile per tick). Memory concerns deferred. Do not mention real-world scale (1 tile = 1 km) in code for now.
- Path store: no change now. Keep calling it as if it stored paths (it recomputes each request). Real store / reachability acceleration structure is future work.
- Armies / workers keep crossing water until a proper embark mechanic exists (islands must not be cut off).

- Pathing: store and its usage unchanged; only the pathfinder output changes (paths movable in straight lines).
- Client rendering: simple, no smoothing / interpolation; entity icons offset by their fractional location (added to the rendered entity struct).
- Off-road must be slower than today, road speed equal to today's speed.

## Tasks

1. [DONE, unbuilt] Fractional foundation: `fractional_loc` -> i16 per axis, 1000 units / tile; `travel_speed` ui16, speed 4 -> 200; GDO, `backend.ts`, `render_entity` (19 bytes) carries the offset. Diagonals ~4% faster than before (integer truncation); no other pace change. TS build regenerates `web/src/*.js`.
2. [DONE, unbuilt] Pathfinder: A* unchanged (8 dir), then cost-checked line-of-sight merge (`PATH_MERGE_MAX_STEPS` 64, supercover, merge only if line cost <= A* subpath cost); no params flag, no corner-cutting rule; `world_path_next_tile` returns first tile on the line, `world_path_next_point` added. Possible later: "direct path" mode ignoring cost. Noted: `path_heuristic` may use `dx+dy-diagonal` where `dx+dy-2*diagonal` seems intended (check with user).
3. [DONE, unbuilt] Movement: fractional straight-line stepping toward `move_target` (= current waypoint), step = round(travel_speed / max(1, cost of current tile)), `ia_isqrt` added to core; re-path on waypoint arrival or destination change (new field `path_destination`, set by `world_movement_stop`); speeds are `army|worker|caravan_travel_speed` in game_parameters (200). Arrival = tile equality, entity stops at tile entry (up to 500 units off center). Stale heuristic comment fixed. Road builder: crew starts ~half a tile early (cosmetic).
4. [DONE, unbuilt] Client: `render.ts` offsets entity icons, selection, troop count and click picking by `fractional / 1000` tiles (check Y sign when testing).
5. [DONE, unbuilt] (helper `path_segment_walk_*` / `world_path_expand_tiles`; road builder anchors per segment (`build_road_data.anchor`), path still recomputed every tick; preview `tile_mode` param, used by `road_building.ts` only; parity already held, no territory guid passed anywhere; `world_path_next_tile` now unused) Tile-bound consumption of segments: helper yielding the supercover tiles of a path segment (same traversal as the merge pass); road builder lays roads along the true line (stable anchor, not greedy from the head); preview gets a tile-following mode (roads, tile-bound uses) vs straight (movement); builder and preview use identical params / helper / scratch size.
5b. [DONE, unbuilt] Thin 8-connected tile walk for roads / tile preview (`path_segment_walk_thin`, tie = lowest coordinates, x then y, walked from the lexicographically lowest endpoint); `path_params.require_complete` (partial / truncated / impassable start = failure) with canonical search direction (lower endpoint first, waypoints reversed, cost recomputed); road builder and tile-mode preview set it. Failed path ends the road project (first day already paid). Known: after the anchor moves, the remainder is a new search and may differ by direction.
5c. [DONE, unbuilt] Reverted 5/5b road logic to HEAD behavior (A* tile route, road builder asks from head, `world_path_next_tile` run semantics, `build_road_data.start`); thin walk removed. New `path_params.straight_segments` (default false = direction-change waypoints / tile runs; true = merged straight segments): set by entity movement (`ARMY_WORKER_PATH_PARAMS`, `CARAVAN_PATH_PARAMS`) and the default preview. Roads and tile-mode preview leave it false, with `require_complete` + canonical search (same tile route from either end; remainder after the head advances may still differ).
5d. [DONE, unbuilt] Coast corner stall fix: `process_entity_movement` slides along the blocked edge when the landed tile is blocked (planner merge allows corner cuts, stepping drifts onto an unchecked side tile). Notes: `world_sim/.ai_imprint/caravan_coast_blocking_survey.md`. Not done: stuck-timeout safety net, stricter line merge (option B), exact segment tracking (in `path_commitment_rewrite`).
6. Road flag per segment: speed on road is a property of the move segment, not the standing tile. The pathfinder splits a segment into road / non-road sections by inserting waypoints as the path is generated (must fit waypoint capacity; losing range is acceptable); each segment carries a road flag; movement uses road speed for a whole road segment and per-tile cost otherwise. Do after task 5, once diagonal roads are tested. Terminology: "segment" (not "leg").

## Deferred

- Path preview draws point to point, so it ignores tile shape. Needs a path parameter: straight lines for movement, tile-following (tile runs) for roads and other tile-bound uses.
- Heuristic chamfer fix applied (unbuilt): `5*(dx+dy-2d)+7d` (was `5*(dx+dy-d)+7d`, overcounted diagonals). Weight 2 unchanged. Re-check: long diagonal paths (partial results, node table limits), per-tick pathing cost. Stale comment in `world_pathing.cpp` says plains/road cost ratio 3 (it is 5 now).

## Decisions (arrival)

- Entities arrive when anywhere on the destination tile (not its center); keep as is. Later: orders targeting an exact position inside a tile (smoothness only).

- Reachability checks: unreachable road costs the crew's first day (`BUILD_ROAD` validity has no reachability test); client preview `require_complete` is coupled to `tile_mode`. Left as is: the real fix is a path store (`backlog/path_store_cache_2026-10-15.md`) so the worker computes the whole route before its first pay, and the road route is stored once per project (also makes roads direction-independent end to end).
- Cleanup: unused `world_path_next_tile`.

## Open questions

- `path_destination` field vs resetting `move_target` through a `world_movement_set_destination` helper (user asking; builder used the field).
- Roads still built in a staircase (road builder uses `world_path_next_tile` from the current head: greedy major-axis-then-diagonal step, not the true line). Needs tile-bound consumption of the line (see Deferred).

## Next step

Confirm speed scale (investigation: speed 4 = 40 of 200 sub-units per tick = 0.2 tile/tick today, so same-as-today on roads = 200 units/tick, i.e. x50 not x100). Then dispatch task 1.
