# WORK UNIT: Client Input

Started: 2026-09-27

Goal: prepare the web client for a real UI, unit selection, and camera
panning, by building a proper camera/viewport and input system.

## Tasks

- [x] Client backend cleanup pass: split the monolithic wasm client file into
  a pure C++ backend (`src/client_web/game_client_backend*`) and a thin WASM
  platform layer (`src/platform/wasm_web_client/`), state passed explicitly
  via a `client_backend&` instead of file-scope statics.
- [x] Camera / viewport: pan + zoom, occlusion, and screen <-> world
  conversion moved into the client backend (C++) - see Notes.
- [x] Viewport movement extra: zoom in where the cursor is located.
- [x] Generalized gameplay input: `INPUT_EVENT_TYPE` code + packed payload
  struct per event, `game_client_process_input_event(backend, code,
  payload_bytes)` in the public header - see Notes for the buffer/ownership
  split.
- [x] Command queue: `game_client_input_state` holds a `command_queue` arena +
  `command_sequence_builder`; `game_client_input_set_target_loc` pushes a
  `SET_ENTITY_MOVE_TARGET` command onto it, `game_client_output_client_tick_message`
  drains the whole queue into the `CLIENT_TICK` message and resets it - see
  Progress for the `match_command_sequence`/wire-format work this rode on.
- [x] Selection state: client-local "currently selected entity", no new
  network message yet. Design (2026-10-02):
  - Hit-testing and the "currently selected" value stay frontend-local (JS
    already has render-list positions for drawing).
  - `render_entity` gains a `guid` field so the frontend can name what it
    hit-tested against.
  - `game_common`/match code gets per-type query functions
    (`query_entity_state_settlement/caravan/army(match, guid) -> single`),
    validating the GUID's type + liveness and returning the same `::single`
    view struct used for spawn start-state - usable from server, client and
    tests, not just this feature.
  - `game_client_backend` calls the relevant query function on request;
    the wasm platform layer owns when to ask and how to marshal the
    result to JS, since that request/response flow is the platform-specific
    part, not the data access itself.
  - Step 1 [x] (2026-10-02, game_common side): `render_entity.guid`,
    `match_entity_is_valid(match, guid)` (type + index + full-guid compare
    against stored guid), `query_entity_state_*(match, guid, single&) -> bool`,
    command validity check/apply use `match_entity_is_valid` (not the full query).
  - Step 2 [x] (2026-10-02, committed b5b139d): `client_query_entity(guid)`
    wasm export -> `game_client_query_entity` -> packed `entity_full_view`
    (38 bytes: core + per-type union; separate from the `::single` views).
    `backend.ts` `query_entity(guid)`, `render.ts` `pick_entity_at`, selected
    guid held in frontend state, shown in the debug panel only. Left-click
    selects, right-click = move target via `mouse_button_action` (to be
    replaced by contextual input later). No highlight drawn yet.
- [x] Right-click move target (location only): `SET_TARGET_LOC` payload =
  `{entity_guid, i32 x, i32 y}` (12 bytes); backend converts to world loc and
  queues `SET_ENTITY_MOVE_TARGET`; server validates (valid guid, army, owner).
  Verified working 2026-10-02.
- [x] Selection indicator: green outline via `is_selected` param in
  `render.ts` draw functions. Verified.
- [x] Selected entity side panel (`name = value` rows, right side, hidden
  when no selection; `index.html`/`style.css`/`debug.ts`). Verified.
- [x] Smooth rendering: jitter traced to `client_render_state`'s viewport
  fields being rounded to whole world tiles (`i32`/`ui16`) every frame -
  changed to `float` end to end (backend struct, `backend.ts` offsets) -
  see Progress.

## Progress

- Backend cleanup pass done, build passing.
- Camera/viewport implemented: `client_viewport_state` (float position for
  smooth panning, zoom_level lerped between min/max viewport sizes in world
  tiles), `client_backend_apply_viewport_input` (pan scaled by viewport size
  + delta time, zoom eased toward target, corner clamped to world bounds),
  `client_backend_build_render_state` now excludes off-viewport entities
  (fixed 5-tile radius) and positions the rest relative to the viewport.
- Render state rebuild + draw moved off the message-driven path onto a
  `requestAnimationFrame` loop (`core.ts`'s frame-callback delegate), so
  panning/zooming is smooth between ticks. Also fixed a live memory leak
  this uncovered: the tick handler was rebuilding render state without ever
  clearing its arena.
- Web client input now drives the camera every frame (WASD/arrows pan,
  wheel zoom) via the same frame-callback delegate.
- World border and mouse world-location added to the canvas / debug panel.
- Generalized input events: `INPUT_EVENT_TYPE` (`VIEWPORT_CONTROL`,
  `SET_TARGET_LOC`) + one packed payload struct per code in
  `game_client_backend.h`, capped at `INPUT_EVENT_MAX_PAYLOAD_SIZE` (64
  bytes) so every platform knows the max size up front without tracking
  individual struct sizes. The backend function takes `(code,
  payload_bytes)` as plain parameters - it doesn't own or know where the
  bytes live. The WASM platform owns a static scratch buffer sized to that
  constant (`wasm_client_input.cpp`), exposed via
  `client_get_input_event_buffer_offset/size()`; `backend.ts`'s
  `ClientInput` namespace caches both once at `start()`, and
  `get_dataview()`/`commit_input(code)` let `input.ts` write one event's
  bytes and commit it by code, with `commit_input` reporting back (via the
  backend's bool return) if the code wasn't recognized - signals TS/C++
  drift. Old `client_apply_viewport_input` / `client_input_set_target_loc`
  wasm exports collapsed into this single path.
- Client input command queue: `match_command_sequence` (`game_commands.h`)
  had its `player_id` moved out into `match_tick_commands`'s own sequence
  entries (`<match_player_id><match_command_sequence>`), making
  `command_sequence_builder` player-agnostic and reusable as-is for the
  client's local queue; `game_client_input_set_target_loc` now pushes onto
  it via `command_queue_builder.push_command<...>()`, and
  `game_client_output_client_tick_message` drains the whole queue into the
  `CLIENT_TICK` message in one `memcpy`, then clears + re-inits the builder.
  `game_message_payload_client_tick` also now embeds `match_command_sequence
  commands` directly instead of its own separate
  `command_header_count`/`_command_headers_buffer` fields, reusing the same
  struct end to end.
- Bugs hit and fixed along the way: a `sizeof(pointer)` vs
  `sizeof(match_command_header)` typo in `command_sequence_builder::push_command`'s
  debug-only size bookkeeping; `match_tick_commands_builder::push_new_sequence`
  allocating its `match_player_id` at natural (2-byte) alignment instead of
  packed (1-byte), which could insert an invisible pad byte before a
  sequence and desync every reader after the first; and the CLIENT_TICK
  output copy first missing the sequence's own fixed part entirely (so
  `command_count` read back as garbage/zero - symptom: target location
  always landing at 0,0), then over/under-sizing the message once fixed,
  now copying the full sequence while only counting the commands buffer
  (not the sequence header) as `build_game_message`'s extra size, since the
  header's already accounted for by the embedded `commands` member.

- 2026-10-01: jitter root cause confirmed - `viewport.viewport_bottom_left`
  (`i32`) and `viewport_width`/`viewport_height` (`ui16`) were truncating the
  eased zoom/pan to whole world tiles every frame; since `scaleX`/`scaleY` in
  `render.ts` derive from `viewport_width`/`height`, that truncation caused a
  visible scale jump each time it crossed a tile boundary - worse at high
  zoom, where a 1-tile step is a much bigger relative jump. Changed all three
  fields to `float` (`game_client_backend.h`, `game_client_render.cpp`,
  `backend.ts`'s offsets - struct grew by 4 bytes).

## Notes / Decisions

- `entity_guid` is a plain 4-byte `ui32` with shift/mask accessors
  (`get_type/get_index/get_extra`, `make`); no C++ bitfields (layout differs
  across compilers). Macro: 8 type / 12 index / 12 extra; micro: 8 / 16 / 8.
  `operator==` covers the whole value. Spawn computes the guid once (extra =
  tick-based) and stores/returns the same value.
- `render_entity` is 16 bytes (`static_assert`ed); `backend.ts` reads guid as
  u32 at +3.

- Camera/viewport ownership moves to the client backend (C++), not JS:
  it computes the visible world rectangle and places entities in viewport
  space; JS becomes unaware of world size, works in viewport space, and
  only needs world coordinates when targeting a world location (which the
  backend also does the conversion for). Selection can stay JS-side, since
  hit-testing works against viewport-space entity positions already handed
  to it.
- This needs two separate refresh cadences into the backend: tick arrival
  (simulate + rebuild entity data, as today) and animation frame (recompute
  viewport placement of that same data as the camera moves/zooms) - the
  latter needs a new `requestAnimationFrame` loop calling a new "recompute
  viewport" export, independent of the tick-driven path.
- Zoom-to-cursor: backend needs the viewport-space cursor position each
  frame (JS already tracks it for the debug panel); on zoom, shift the
  viewport position so the world point under the cursor stays fixed as
  zoom_level changes.
- An interpolation layer (entities moving smoothly between ticks) is also
  planned, likely riding the same frame-driven path as the camera.
- C++ iteration cost (rebuild/redeploy per change) for tuning camera feel
  is accepted: the client already needs a redeploy on every change today,
  so this doesn't change the loop - may revisit if the server gains the
  ability to refresh servable files without a restart.
