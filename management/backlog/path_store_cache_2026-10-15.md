# Path store and cache

Input date: 2026-10-15

## Item

Real world path store: multiple stored paths, reference-counted handles, invalidation,
cache reuse (key on (start, end) or on regions: undecided).

## Why

Only needed when performance demands it: entities currently recalculate a path at every
tile step (full search, ~128 KiB arena zeroed each time).

## Current state

- Minimal `world_path_store` keeps only the last path; `world_entity_movement.path_handle`
  always `INVALID_PATH_HANDLE`; `path_result.handle` unused.
- Origin: `work_units/settlement_trade_construction_2026-10-03.md` (former task 4).
- Invalidation will be needed once paths persist (terrain / road changes).
