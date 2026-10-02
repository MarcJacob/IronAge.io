# WORK UNIT: UI & first commands

Started: 2026-10-02

Goal: first interactive UI (DOM overlay) driving real game commands. Test-grade
mechanics only; no game-design rules in this unit.

## Decisions

- Founding has no location / time conditions. New settlement: army's location,
  army's owner, population = total army manpower, other fields spawn defaults.
- Founding destroys the army in the same command (one command, atomic).
- Local player id (from `MATCH_JOINED`) stored in the frontend (Main module or
  a more fitting one).
- Frontend architecture: new `game_ui` TS module = in-match UI (HUD overlay),
  an extension of `input.ts` (imports from it, e.g. selected entity). Command
  sending logic stays in `input.ts` (exported send functions, triggerable from
  anywhere); `game_ui` only generates the signals. Global non-backend state
  (local player id) lives in `main.ts`.
- HUD: `#hud` container `pointer-events: none`, widgets `pointer-events: auto`;
  canvas input handlers ignore events not targeting the canvas.
- Delegation split: (A) C++ input event + `input.ts` sender + canvas-target
  check; (B) `game_ui` module, HUD, panel, button.
- Next button after this one: a settlement spawns an army from its population
  (cost / composition decided then).

## Tasks

- [ ] `FOUND_SETTLEMENT { army guid }` command in game_common: validity (valid
  guid, army, owned by sender), apply (spawn settlement, destroy army).
  Needs an entity destroy path for armies if none exists.
- [ ] UI scaffold: place for buttons / panels, separate from the game canvas
  (DOM overlay, like the debug panel); pointer-events pass through except on
  widgets.
- [ ] Player-facing selected-entity panel in the overlay (replaces the debug
  side panel).
- [ ] "Found settlement" button: shown / enabled only when an army owned by the
  local player is selected; sends the command via the input event path.
- [ ] Button: settlement spawns an army from its population.

## Progress

- 2026-10-02: unit created.
