# BACKLOG: Worker progress and status in the GDO

Input date: 2026-10-26

## Item

Worker panel (`ui_element_worker_panel.ts`) lacks a completion slider and a working status ("Working" green / "Not enough gold" / "Not enough workers" red).

## Facts

- `work_progress_ratio` exists server-side but is never written; not in the worker GDO.
- Building project: `progress_days` in action data. Road crew: no total. Founding: no progress.
- Only the road crew stores `funded` (action data, not exposed). Building gold failure is not stored (known only inside the tick). "Not enough workers" applies to building projects only (their workers are not selectable).
- Only founders and road crews are selectable workers.
- Details: `src/platform/web_game_client/.ai_imprint/panel_gdo_availability.md`.

## Next step

Decide what "progress" means per work type, add the GDO properties + store the pay-failure state, then add the fields to the worker panel.
