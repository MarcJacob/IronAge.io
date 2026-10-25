# Straight-line pathing and movement

Input date: 2026-10-15. Started: 2026-10-18 (moved from backlog; first remainder of `road_construction_2026-10-03.md` task 3).

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

## Next step

Design (not started): how straight-line paths are represented and how cost-based speed interacts with the deterministic tick.
