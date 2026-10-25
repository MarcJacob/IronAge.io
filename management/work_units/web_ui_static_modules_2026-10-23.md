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
- Action bar: static container with ONE template button element cloned from code per action item (temporary example of the dynamic-element approach; all buttons identical).
- Properties panel: one panel per entity type (Settlement, Army, Worker, Caravan), each with static fields, under a shared frame element (title, owner subtitle...). Done after batch 2.
- Batch 2 (2026-10-25): calendar, placement, player state, tooltip, `game_ui` debug panel, action bar; dead-code cleanup from the survey.
- Module init: each new-style module initializes itself at import (top-level call at the bottom of its file); no init calls in `main.ts`. A module nothing imports is not needed, so not initialized. (Builder delegated 2026-10-25.)
- Main menu screen is part of this overhaul (kicked players return to it).
- Input: keydown handler must ignore events targeting `<input>` / `<select>`.

## Pilot result (built, hand-tested)

- Template (copy `ui_element_login.ts`): `IDS` const at top, `XState` interface, `init()` caching elements once, `submit(state | null)` (null hides; writes only on change), `on(event, cb)` via `ui_events.ts` (`UiEvents`), namespace imports. Notes: `web_game_client/.ai_imprint/adding_ui_things_howto.md`.
- Driver mode owns module subscriptions (`lobby_base.ts`).
- Edit guard for inputs: focused or recently changed control keeps the user's value.
- `front_src.imprint.md` still describes the old style (user to update).

## Batch 2 (done, unbuilt)

- Migrated: calendar, placement, tooltip, player state (static fields), `game_ui` (HUD + debug panel, event `toggle_debug`), action bar (template button clone, event `action` with item id; mode owners subscribe on entry / unsubscribe on exit).
- Debug panel collapsed state now resets per match entry; placement / tooltip files keep old names.
- `front_src.imprint.md` UI section is stale (user to update).

## Properties panel design (user, 2026-10-25)

One static panel per entity type under a shared frame (title, owner subtitle).

- Settlement:
  - Top left: row 1 population count + colored growth percent on the same row (`+` green growth, `-` red shrinkage, yellow when above but close to 0). Row 2 production total + small tax slider beside it. Row 3 "Consumption" header: sold amount / consumption requirement. Row 4 goods pool.
  - Top right: "Garrison" with troop type sliders.
  - Bottom: buildings, centered; NOT templated (static elements, as a test).
- Army: same as today + an "origin" settlement row (like caravans).
- Caravan: same as today.
- Worker: title "Settlers" when its current build type is founding a settlement (if the front can know via GDO); completion slider; status "Working" (green) / "Not enough gold" / "Not enough workers" (red), if the front can get it via GDO.

## Interim panel tweaks (done, unbuilt)

- Population row: signed daily growth percent (server `population_growth_rate / 10`), green / yellow (< 0.5%) / red; `delta_text` / `delta_tone` on the value row.
- "Goods sold" row: delta = surplus / deficit (sold - needed).
- Modules self-initialize at import (done, hand-tested).

## GDO availability (survey done; note `web_game_client/.ai_imprint/panel_gdo_availability.md`)

- Settlement: everything available (tax readable + settable via `SET_SETTLEMENT_TAX_RATE`, limits 0..10).
- Army: everything available; `home_settlement` already read, needs an origin row mirroring the caravan's.
- Caravan: available; GDO has no `owner` (subtitle empty).
- Worker: `work_type` (0 found settlement, 1 road, 2 building) available, needs a `WORK_TYPE` mirror enum in `backend.ts`; GDO has no `owner`; completion fraction missing (`work_progress_ratio` exists server-side but is never written; building has `progress_days`, road crew has no total, founding has no progress); working status missing (only the road crew stores `funded`, not exposed; building gold failure is not stored; "Not enough workers" only applies to building projects, whose workers are not selectable).
- Only founders and road crews are selectable workers.
- `settlement_founding.ts` submits its own panel, bypassing `inspect_entity`.

## Decisions (panel scope)

- Worker panel: show what exists now (title "Settlers" via `WORK_TYPE` mirror enum / "Road crew", origin, location); progress + status GDO props are a later server-side task.
- Owner of caravans / workers: helper in `world_entities.h` (like the location helper) derives it from the origin; GDO exposes `owner` for them.

## Panel migration steps

1. [DONE, unbuilt] Shared frame + Caravan + Army panels (`ui_element_entity_frame.ts`, `ui_element_caravan_panel.ts`, `ui_element_army_panel.ts`, helpers `static_rows.ts`). Routing in one place: `inspect_entity.ts` `show_entity(view | null, context)` (Army / Caravan -> frame + panel, else old panel) and `show_generic_panel(state | null)` (mode-built panels, hides frame). Army sliders via context `edit_raise_targets` + `raise_targets` event. Origin row added to Army. Owner helper `world_entity_get_owner` added and caravan / worker `owner` GDO props (unbuilt); C++ duplicates replaced.
   - "Stop raising" stays in the action bar. Settlement "Raise army" panel stays settlement-side until step 2.
   - Old code to delete after step 3: old `ui_element_properties_panel.ts` + row classes, `inspect_army`, `inspect_caravan`, `army_count_section`, `army_supplies_section`, `caravan_section`, `on_raise_targets` path, `init_ui_element_properties_panel`; move `supplies_color`.
   - `basic_section.ts` TS owner lookup via origin is now redundant (remove at step 3).
   - Stale imprints (user): `inspection.imprint.md` (panel "generic on purpose" now false), `front_src.imprint.md` UI section.
   - Dead army / caravan code removed (unbuilt); left: `on_raise_targets` (settlement Raise-army path), unused `army` branch of `army_raise_section`.
2. [DONE, unbuilt] Settlement panel (`ui_element_settlement_panel.ts`: events `tax_rate`, `ceiling(type, value)`, `build(type)`; context `edit_settlement`, `buildable[]`). Buildings static (3 boxes: barracks, caravanserai, trade depot) + one hidden template for project bars. Settlement with `context.raise_targets` routes to the old Raise-army panel.
   - Dead (not deleted): `section_economy.ts`, `section_construction.ts` (whole files), `settlement_pool_section`, `trade_pool_section`, `basic_section.ts`, `inspect_generic` (only unknown types).
   - Still used by old panel until Raise-army + founding migrate: `ui_element_properties_panel.ts` + row classes, `inspect_settlement_raise`, `on_raise_targets`, `army_raise_section` / `army_raise_cost_section`, `init_ui_element_properties_panel`.
3. [DONE, unbuilt] Worker panel (`ui_element_worker_panel.ts`, title Settlers / Road crew / Builders by `WORK_TYPE`, origin, location) + Battle panel; built in the step 2 run.
4. [DONE, unbuilt] Raise-army panel (`ui_element_raise_army_panel.ts`, event `raise_targets`) + Found-settlement panel (`ui_element_found_settlement_panel.ts`); routing in `inspect_entity.ts` is a single `show(Shown | null)` (exports `show_entity`, `show_raise_army_panel`, `show_found_settlement_panel`); unknown types show the frame alone. Old generic panel code, markup and CSS removed.
   - Files to delete (user), `front/src/ui/inspection/`: `ui_element_properties_panel.ts`, `ui_element_gauge.ts`, `ui_element_text_row.ts`, `ui_element_value_row.ts`, `ui_element_slider_row.ts`, `ui_element_building_row.ts`, `section_basic.ts`, `section_economy.ts`, `section_construction.ts`; also stale compiled `.js` in `web/src/`.
   - `TROOPS` `label` / `color` entries unused.

## Properties panel rebuild: complete once built and hand-tested.

## Open questions

- Later server task: worker progress + status GDO props.
- Stale imprint `front_src.imprint.md` UI section (user).

## Next step

Complete: built and hand-tested by user (2026-10-26). Leftovers: worker completion / status fields (server GDO task), `front_src.imprint.md` update (user).
