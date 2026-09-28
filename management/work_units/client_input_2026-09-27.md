# WORK UNIT: Client Input

Started: 2026-09-27

Goal: prepare the web client for a real UI, unit selection, and camera
panning, by building a proper camera/viewport and input system.

## Tasks

- [ ] Camera / viewport: pan + zoom, screen <-> world conversion goes through
  it instead of `render.ts`'s current fixed full-map scale.
- [ ] Input system: separate raw input capture from interpretation (mode:
  move order, select, pan, ...).
- [ ] Selection state: client-local "currently selected entity", no new
  network message yet.
- [ ] UI scaffold: place for buttons / panels, separate from the game
  canvas (DOM overlay, like the debug panel).

## Progress

(nothing yet)

## Notes / Decisions

(design decisions go here as they're made)
