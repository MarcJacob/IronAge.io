# Web client UI: static DOM, module-bound

Created: 2026-10-23. To start right after `army_supplies_2026-10-23.md`. Multi-file client architecture change. Related (separate) backlog item: `web_client_overhaul_2026-10-23.md` (generated client, DOM naming rules).

## Problem

- Panel elements are built dynamically and reused/filled generically; every element must follow a pre-established format, which makes changes hard. Little value gained.

## Direction

- Each UI module is associated with a specific DOM element in the HTML that has pre-established fields, which the module hides / shows / fills.
- The module file is the bridge between logical state and DOM values.
- Unchanged: modules expose subscribable events; modes set module states.
- Dynamic content (lists etc.) still gets built in TS from template elements.

## Tasks

1. Read-only survey: current UI module system (`front/src/ui/`), how panels/elements are built, event/mode wiring, CSS; write to a `.ai_imprint`.
2. Design the target module shape (user decision) and migration order.
3. Migrate module by module.

## Decisions

- Module = bridge between its specific DOM elements (extracted once at init) and modal logic. Static fields filled throughout the interface; nothing is generic / data-driven (`PanelRow` union goes away).
- Even buildings are static elements (one per building type); no known element needs to be dynamic (no leaderboard, few item lists). Build dynamically only for genuine lists, from templates; generation of static markup may be automated later.
- Show / hide at both levels: a whole module and individual elements inside it, per module.
- Single `index.html` for now; multiple HTML pages likely later.
- Old and new styles coexist during migration.
- Migration order: small modules first (calendar, placement, lobby, player state, tooltip, action bar; lobby early for admin controls), properties panel last.
- Properties panel: probably one panel per entity type, sharing a parent framing element.
- Menu / lobby DOM stays in the same `index.html` (single websocket for the whole flow; separate pages only after a master / match server split, not before end of Alpha).
- Pilot (2026-10-25): login, lobby list, lobby (+ test-mode admin controls) built as the first new-style modules; match end returns the client to the lobby list. Calendar and the rest migrate later.
- Main menu screen is part of this overhaul (kicked players return to it).
- Input: keydown handler must ignore events targeting `<input>` / `<select>`.

## Pilot result (built, hand-tested)

- Template (copy `ui_element_login.ts`): `IDS` const at top, `XState` interface, `init()` caching elements once, `submit(state | null)` (null hides; writes only on change), `on(event, cb)` via `ui_events.ts` (`UiEvents`), namespace imports. Notes: `web_game_client/.ai_imprint/adding_ui_things_howto.md`.
- Driver mode owns module subscriptions (`lobby_base.ts`).
- Edit guard for inputs: focused or recently changed control keeps the user's value.
- `front_src.imprint.md` still describes the old style (user to update).

## Open questions

- Next migration batch (calendar, placement, player state, tooltip, action bar) once the pilot is hand-tested.

## Next step

Task 1 survey builder running (2026-10-25), output to `web_game_client/.ai_imprint/ui_module_system_survey.md`. Then user decides target module shape (task 2). Lobby admin controls (`server_test_mode`) build on it.
