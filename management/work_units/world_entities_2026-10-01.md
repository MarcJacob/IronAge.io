# WORK UNIT: World Entities & Settlement Rendering

Started: 2026-10-01
Closed: 2026-10-02

Goal: begin the "World & settlements simulation" phase - world state entity
storage (settlements/armies/caravans), spawning, and the first real entity
rendering (settlement icon) in the web client.

Closed: storage, spawning and rendering now exist for all three entity types
(settlements, armies, caravans), which was the full original goal. Picked up
along the way: a frontend-owned camera (replacing the backend-owned one) and
some placeholder world-simulation mechanics - both already marked as their
own follow-on concerns below rather than folded into this unit's goal.

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
- [x] Caravan entity storage + spawning (`world_entity_caravans`,
  `match_spawn_caravan`), alongside the army entity type added earlier.
  `entity_guid`'s inner union fields renamed `_static`/`_dynamic` ->
  `_macro`/`_micro` to match the MACRO/MICRO terminology already used in
  `world.h`'s comments; added `entity_guid::is_valid()` and `operator==`.
- [x] Army + caravan rendering: `entity_army.svg`/`entity_caravan.svg`, and
  `render.ts`'s settlement-only `draw_entity_settlement` generalized into a
  single `draw_entity()` plus a `RESOURCES_STORE` sprite table keyed by
  entity type, so adding a new rendered entity type no longer means adding a
  new draw function.
- [x] Camera ownership moved from backend (C++) to frontend (new
  `camera.ts`): the frontend now computes the world-space view rectangle
  (pan/zoom/easing, flagged in-code as AI-generated-and-reviewed-only) and
  sends it down each frame; `client_viewport_state` shrank to just
  `view_rect_min`/`view_rect_max`, and the backend's old
  `client_backend_get_viewport_size` pan/zoom-level math was deleted - see
  Notes, this reverses a decision recorded in `client_input_2026-09-27.md`.
- [x] `command_set_entity_move_target`'s validity check / apply are
  implemented for `ARMY` (were TODO stubs before): checks the issuing player
  owns the army, then sets its `movement.move_target`.
- [x] Temporary, explicitly-TEMP world-simulation mechanics riding on the new
  entity types, to have something to look at/test against: an army spawns
  near each settlement at match start; settlements periodically spawn a
  caravan toward another settlement; caravans and armies step toward their
  move target with simple Manhattan movement each tick; caravans despawn and
  bump origin + destination local wealth on arrival. All marked TEMP/TODO in
  `game_common_main.cpp` pending a dedicated, multithreadable world-sim
  subcomponent - not part of this unit's original goal, tracking it here
  only because it rode in on the same entity-type work.

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
- Settlement icon served from `web/art/settlement.svg` (since renamed
  `entity_settlement.svg` alongside the army/caravan icons), registered in
  the server's explicit `WEB_FILES` list (no auto-discovery yet - see
  `backlog/platform_list_resource_files_2026-09-20.md`); added `svg` ->
  `image/svg+xml` to the web server's content-type table (previously
  unsupported, would have served as `application/octet-stream`).
- `world_entity_container`'s separate `bool* _activeFlags` array replaced by
  `entity_guid* guids`, zeroed (`INVALID_ENTITY_GUID`) for inactive slots -
  the GUID slot now doubles as its own validity flag via `is_valid()`,
  removing a parallel array that had to stay in sync with it.
- Bug fixed along the way: the caravan destination-settlement scan's
  validity check was double-negated (`!settlements.guids[destIndex]
  .is_valid() == false`), which is equivalent to `.is_valid()` - the loop
  was accepting the first *invalid* slot as a destination instead of
  skipping it. Fixed to a plain `!...is_valid()`.
- Bug fixed: the army spawned at each match start was given its settlement's
  un-offset location as its initial `move_target` while being placed at an
  offset spawn location, so every starting army immediately marched onto its
  settlement. Fixed so spawn location and initial move target both derive
  from the same offset `startLoc`.

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
- Camera/viewport ownership moved to the frontend: `camera.ts` now owns
  pan/zoom/easing and hands the backend a plain world-space view rectangle
  (`view_rect_min`/`view_rect_max`) each frame; the backend only has to
  decide what's visible and where within that rectangle, no longer anything
  about pan speed, zoom easing, or min/max zoom span. This is the opposite
  of the ownership split recorded in `client_input_2026-09-27.md`'s Notes
  ("Camera/viewport ownership moves to the client backend, not JS") - that
  work unit's notes are now stale on this point and should be read with this
  reversal in mind.
- `command_set_entity_move_target_apply`'s `switch` has no `break` after the
  `ARMY` case, falling through into `default: break;` - harmless today since
  `default` does nothing but `break`, but worth a real `break` if `default`
  ever grows a body.
