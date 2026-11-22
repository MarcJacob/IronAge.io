# Pathing rewrite

Started: 2026-10-29. Absorbs `backlog/path_store_cache_2026-10-15.md` (to move here when store work starts). Straight-line movement (speed x50 on roads vs today) is re-created on top of this system. Plan: `src/game_common/world_sim/.ai_imprint/pathing_cleanup_plan.md`.

## Goal

Clean slate, then user (by hand / peer) builds pathing with acceleration structures held by the world's path store: land / sea geo graph, connected components for reachability and higher-level search, road networks, settlement-to-settlement caches.

## Skeleton (decided)

- Generalist pathfinder `world_path_find(terrain, params, scratch, out_path)`: used by the client preview. Stub for now.
- Path store (world-owned), only these used across the sim: (a) get path A to B -> handle; (b) entity movement: takes the entity movement struct, returns bool (false = movement over / impossible); (c) next tile on path (handle, current tile, destination), used by road crews.
- One enum of path modes replaces mode + flags (`PATH_MODE`: ANY, LAND, LAND_ROADS_ONLY, LAND_PREFER_ROADS, SEA). `path_params {start, end, mode, within_settlement_territory}`.
- Entities keep: sub-tile location, travel speed, path handle (+ `destination`). Road crews hold the handle in `build_road_data`.
- `require_complete` / canonical direction dropped; may return in another form.

## Architecture

Pathing system must optimize for speed of reachability check and travel cost estimate, not necessarily exact path finding.
Path optimization over long distances is secondary. Roads make it cheap.

- World path store initialization = create acceleration structures.
    - Path areas. Build a grid, identify land and sea components by reachability.
        - Construction: Each tile given a local component index. Flood fill with lowest component ID by type depending on start tile type. Keep track of which components remain. 8 bits (so grid mesh size is 16x16).
        - Vast majority of grid tiles will have one (land / sea) or two (coast) components, sometimes three (straights, lakes...).
        - Determine reachability graph. Each local component can link to neighbouring components of the same time if they both touch a common grid edge.
        - Connections are double sided so only stored once per pair. Number of pairs depends on overall complexity of the graph, gets bad if there are a lot of small land / sea areas everywhere.
            - Total size is variable. Max allowable size for world to be validated ?
            - Total connections count for flat world (only land or only sea) of grid dimensions d x d converges to 4^log2(d).
                - For 8 x 8 regions, d = 64 x 64, something close to 4^6 = 4096 connections.
        - Build global land / sea reachability components areas belong to. "Continents" and "Seas".
            - Reachability test = compare components. Extremely fast.
        - Reserve space for caching paths & itineraries (composite paths). Paths go from area to area, path travellers go to start and end points on their own with no caching (path following functions take care of it).
        - Updates to graph: Never in reachability (can't build bridges or canals), can be updated in terms of cost (roads) meaning paths can expire.
        - Store average / lowest cost on area connections ? Allows computing rough travel cost over long distances for much cheaper.

## Tasks

1. [DONE] Teardown to skeleton: stubs in `world_pathing.cpp`, `build_road_data.path_handle` added, client query path synced.
2. Rewrite pathfinder + store internals (user, by hand / peer).
   - 2a. [WIP, user by hand] Acceleration structure: path areas on a tight grid over the world; weighted connections between areas (each with a cost); area ID per tile; reachability component (land or sea) per area.
   - 2b. Pathfinder on top of it.
   - 2c. Store interface (handles, movement, next tile), plus the internal work for the path cache.

## Next step

Finish 2a (user, by hand), then 2b / 2c.
