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

- [x] `FOUND_SETTLEMENT { army guid }` command in game_common: validity (valid
  guid, army, owned by sender, free settlement slot), apply (spawn settlement,
  destroy army via `match_destroy_army`).
- [x] Input path: `INPUT_EVENT_TYPE::FOUND_SETTLEMENT` (payload `{entity_guid}`),
  `send_found_settlement(army_guid)` in `input.ts`; canvas handlers ignore
  events not targeting the canvas.
- [x] UI scaffold: `game_ui.ts` + `#game_hud` overlay (pointer-events none,
  widgets auto); local player id in `main.ts`.
- [x] Player-facing selected-entity panel in the HUD (debug side panel removed).
- [x] "Found settlement" button: shown only when an army owned by the local
  player is selected.
- [x] Canvas fills the window, no scrolling, uniform tile scale (camera view
  height is the zoom value; width = height * canvas aspect); HUD CSS follows
  the container. Frontend-only: the TS camera owns the world rectangle sent
  to the backend.
- [x] `SPAWN_ARMY { settlement guid }` command (cost 100 population -> army of
  100 levies, same location / owner) + "Spawn army" HUD button (owned
  settlement, population >= 100); `send_spawn_army` in `input.ts`.
- [x] `match_settlement_decrease_population` (saturating; destroys the
  settlement at 0 via `match_destroy_settlement`).
- [x] Test-only population growth: 1% chance per tick per settlement, +5%
  (min 1). Throwaway mechanic.
- [x] Caravan code hardened against destroyed settlements (destination pick
  no longer assumes dense slots; caravans despawn if destination is gone).
- [x] Selection cycles through entities stacked under the cursor
  (`pick_entity_at` in `input.ts`: next hit in render-list order after the
  selected one).

## Progress

- 2026-10-02: unit created.
- 2026-10-03: unit complete, all verified working by Marc. Found settlement spawns
  with zeroed wealth / tier / trade attractivity / area influence (no defaults
  function exists yet).
