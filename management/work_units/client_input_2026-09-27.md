# WORK UNIT: Client Input

Started: 2026-09-27

Goal: prepare the web client for a real UI, unit selection, and camera
panning, by building a proper camera/viewport and input system.

## Tasks

- [x] Client backend cleanup pass: split the monolithic wasm client file into
  a pure C++ backend (`src/client_web/game_client_backend*`) and a thin WASM
  platform layer (`src/platform/wasm_web_client/`), state passed explicitly
  via a `client_backend&` instead of file-scope statics.
- [ ] Camera / viewport: pan + zoom, screen <-> world conversion moves into
  the client backend (C++), not `render.ts` - see Notes.
- [ ] Input system: separate raw input capture from interpretation (mode:
  move order, select, pan, ...).
- [ ] Selection state: client-local "currently selected entity", no new
  network message yet.
- [ ] UI scaffold: place for buttons / panels, separate from the game
  canvas (DOM overlay, like the debug panel).

## Progress

- Backend cleanup pass done, build passing.

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
- An interpolation layer (entities moving smoothly between ticks) is also
  planned, likely riding the same frame-driven path as the camera.
- C++ iteration cost (rebuild/redeploy per change) for tuning camera feel
  is accepted: the client already needs a redeploy on every change today,
  so this doesn't change the loop - may revisit if the server gains the
  ability to refresh servable files without a restart.
