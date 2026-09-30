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
- [ ] Command queue: adapt entity target-location input (currently the
  `input.pending`/`pending_target` single-slot temp state, see
  `game_client_input_state`) to push onto a proper match command queue on
  the client backend, instead of a lone pending flag; `game_client_process_input_event`'s
  `SET_TARGET_LOC` case pushes a command there instead of setting
  `input.pending` directly. `game_client_output_client_tick_message` then
  reads/drains that queue to build the `CLIENT_TICK` command sequence,
  instead of building a single hardcoded `SET_ENTITY_MOVE_TARGET` command
  from `input.pending_target`. General shape per-event going forward:
  receive code + payload -> interpret/validate -> consume directly (like
  viewport input) and/or push a match command onto the queue for the next
  `CLIENT_TICK`.
- [ ] Selection state: client-local "currently selected entity", no new
  network message yet.
- [ ] UI scaffold: place for buttons / panels, separate from the game
  canvas (DOM overlay, like the debug panel).
- [ ] Smooth rendering: currently jittery when zoomed in close - see Notes.

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

## Notes / Decisions

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
- Jitter at high zoom: unconfirmed cause yet - candidates are sub-pixel
  rounding of viewport/entity positions before draw, and/or entities
  needing the interpolation layer above rather than snapping tick-to-tick.
  Needs investigation before picking a fix.
- C++ iteration cost (rebuild/redeploy per change) for tuning camera feel
  is accepted: the client already needs a redeploy on every change today,
  so this doesn't change the loop - may revisit if the server gains the
  ability to refresh servable files without a restart.
