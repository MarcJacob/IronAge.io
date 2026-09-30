# WORK UNIT: World Entities & Settlement Rendering

Started: 2026-10-01

Goal: begin the "World & settlements simulation" phase - world state entity
storage (settlements/armies/caravans), spawning, and the first real entity
rendering (settlement icon) in the web client.

## Tasks

- [x] World entity storage: SoA property arrays per entity type
  (`world_entity_settlements`/`armies`/`caravans`, `include/game_common/match/world.h`),
  a shared `world_entity_container` base (`max_count`/`_activeFlags`), a
  `single` view struct per type for spawning/read access, `world_alloc_*`
  functions (with per-allocation null-checks), wired into `world_state_init`.
- [x] Spawn functions: `match_spawn_settlement`/`match_spawn_caravan`/`match_spawn_army`
  (`game_common_main.cpp`) - free-slot scan, active flag, deterministic
  `entity_guid`, start-state distributed across the SoA arrays. See Notes for
  the GUID shape.
- [x] Settlement rendering: `render_entity` reworked to a general shape
  (`entity_type`/`owner`/`viewport_location`/`size_viewport`) instead of the
  old fixed move-target-only shape; `game_client_rebuild_render_state`
  populates it from live settlements; `art/settlement.svg` (hand-drawn
  placeholder) served by the web server and drawn via `draw_entity_settlement`
  in `render.ts`, tinted green when locally owned.
- [x] Page layout pass: removed the WebSocket test button/panel from the page
  (test code itself untouched, just unhooked - its own init already
  null-guards missing elements), centered + enlarged the canvas (800x800 ->
  1200x800), moved the debug panel to a full-width footer below the canvas.
- [ ] Viewport aspect ratio awareness - widening the canvas exposed that
  viewport sizing doesn't know the canvas's pixel aspect ratio, so tiles
  stretch non-uniformly - `backlog/viewport_aspect_ratio_2026-10-01.md`.

## Progress

- Bugs found & fixed along the way:
  - `ia_rand_range`'s `#if __STDC_HOSTED__` branch silently picked the
    freestanding stub (`return min;`) even on the native server build - MSVC
    doesn't reliably define that macro for C++. Replaced both branches with a
    hand-rolled xorshift32 PRNG so this can't diverge by compiler again; also
    fixed `ia_rand_vec` ignoring its own `min` parameter.
  - `game_match_start_params::get_settlement_start_state` ignored its `index`
    parameter entirely - every settlement read/wrote the same struct. Combined
    with the RNG bug above, every settlement ended up spawned on top of
    whichever one was written last. Fixed to index into the array.
  - `world_alloc_settlements` was missing `return true;` (declared `bool`,
    never returned); all three `world_alloc_*` functions were never
    allocating `_activeFlags` at all (would have null-deref'd on first
    spawn); `match_spawn_settlement`'s free-slot scan looped forever on an
    occupied slot (`continue` without incrementing `freeIndex`);
    `entity_guid._type` was never being set on spawn (left every entity's
    GUID untyped).
- Settlement icon served from `web/art/settlement.svg`, registered in the
  server's explicit `WEB_FILES` list (no auto-discovery yet - see
  `backlog/platform_list_resource_files_2026-09-20.md`); added `svg` ->
  `image/svg+xml` to the web server's content-type table (previously
  unsupported, would have served as `application/octet-stream`).

## Notes / Decisions

- `entity_guid` distinguishes MACRO entities (settlements: 12-bit index/extra
  bitfields via `_static`) from MICRO entities (armies/caravans: full 16-bit
  index + 8-bit extra via `_dynamic`) - matches the much higher expected
  count for micro entities documented in `world.h`.
- `world_entity_settlements::active_count` is intentionally simple for now:
  it relies on nothing ever despawning yet, so active settlements are always
  exactly `[0, active_count)`, letting consumers stop scanning early. Will
  need revisiting once despawn exists (freed slots create gaps, breaking the
  contiguous-range assumption).
- Settlement icon drawing is a plain `drawImage` in `render.ts`, no asset
  pipeline beyond that - this is the first image asset the frontend has ever
  loaded (everything before was procedural canvas drawing).
