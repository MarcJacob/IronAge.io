# Path commitment / pathing rewrite

> 2026-10-29: superseded by `pathing_rewrite_2026-10-29.md`. History only.

Raised: 2026-10-22 (from the pathing direction investigation; see `src/game_common/world_sim/.ai_imprint/pathing_direction_investigation.md`).

- Entities re-path from their current tile on every tile entered and keep only the first step; no stored path (`path_handle` always invalid).
- Store a path per entity, recompute only when destination changes, path is blocked, or every N tiles. Makes the client path preview match the real route and removes flip-flop causes.
- Stop-gap applied 2026-10-22: one constant heuristic weight instead of the 32-tile switch (loses long-distance preview reach; weighted heuristic still non-admissible).
- Related: `path_store_cache_2026-10-15.md` (selection runs one A* per settlement).
- Caravan destination selection (2026-10-23): every spawn / retarget compares all candidate settlements with one pathfinding request each, so cost grows with settlement count. Needs massive optimization before player count can increase (currently 3). Tackle together with the pathing / movement changes.
