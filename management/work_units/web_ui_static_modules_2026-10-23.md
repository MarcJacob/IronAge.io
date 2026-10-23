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

- None yet beyond the direction above.

## Open questions

- Per-module migration order; whether old and new styles coexist during migration (assumed yes).
- Where static markup lives (single HTML vs per-module fragments).

## Next step

After army_supplies: propose the survey builder (task 1).
