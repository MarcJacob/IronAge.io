# Pathing rewrite

Started: 2026-10-29. Supersedes the pathing tasks of `straight_line_movement_2026-10-15.md` (2, 3, 5-5d, 6), and `path_commitment_rewrite_2026-10-22.md`; absorbs `backlog/path_store_cache_2026-10-15.md` (to move here when store work starts). Plan: `src/game_common/world_sim/.ai_imprint/pathing_cleanup_plan.md`.

## Goal

Clean slate, then user (by hand / peer) builds pathing with acceleration structures held by the world's path store: land / sea geo graph, connected components for reachability and higher-level search, road networks, settlement-to-settlement caches.

## Skeleton (decided)

- Generalist pathfinder `world_path_find(terrain, params, scratch, out_path)`: used by the client preview. Stub for now.
- Path store (world-owned), only these used across the sim: (a) get path A to B -> handle; (b) entity movement: takes the entity movement struct, returns bool (false = movement over / impossible); (c) next tile on path (handle, current tile, destination), used by road crews.
- One enum of path modes replaces mode + flags (LAND, LAND_ROAD, LAND_DIRECT, SEA...). Params struct keeps equivalents of the old preview args.
- Entities keep: sub-tile location, travel speed, path handle (+ `destination`). Road crews hold the handle in `build_road_data`.
- `require_complete` / canonical direction dropped; may return in another form.
- Sub-tile stepper and coast-corner slide kept, de-wired.
- Consequences until rewritten: entities stand still, no caravans / ships spawn, no roads built, empty previews.

## Tasks

1. [DONE, build passes] Teardown to skeleton (builder, Sonnet). `PATH_MODE` enum (ANY, LAND, LAND_ROADS_ONLY, LAND_PREFER_ROADS, SEA); `path_params {start, end, mode, within_settlement_territory}`; store stubs in `world_pathing.cpp`; `world_entity_step_towards` (stepper + slide) unused; `build_road_data.path_handle` added (`WORLD_WORKER_ACTION_DATA_SIZE` 16 -> 20, `start` is written but never read: could be dropped instead); `client_query_path(..., territory_guid, path_mode)` and `backend.ts` synced.
2. Rewrite pathfinder + store internals (user, by hand / peer).

## Open questions

- `armies_workers_cross_water` game parameter now unused.
- `ai_roads_survey.md`, `workers_system_overview.md` notes still describe old road logic.
- `fractional_loc` GDO type `VEC2_UI16` vs `vec2<i16>` mismatch in caravans (pre-existing).

## Next step

Start task 2 (user, by hand or with a peer): pathfinder first, then store internals.
