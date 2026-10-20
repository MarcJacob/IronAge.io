# World sim simplification

Started: 2026-10-19. Off the Alpha feature order: done before `basic_world_economy` and the other Alpha units.

## Scope

Game common + client. Strip economy back, tidy army / battle, restructure world sim into subfolders.

## Decisions

- Economy keeps only: population, population growth, territory mechanics.
- Remove local wealth, taxation hooks and caravans entirely (entity type too, for now).
- New settlement folder in world sim for settlement-centric systems.
- Army / battle cleanup: find abstractions linking settlements to army mechanics.
- Roads and construction stay as they are (shape may change later).
- Found settlement is removed (command, validity, UI). Replacement action comes later, likely tied to a specific entity type.
- World sim system files go into proper subfolders.
- Troop pools / ceilings / raising: fate not decided (settlement-army link). Audit to report.

- Entity folders under world_sim: settlements, armies, battles, builders; each with header + cpp (spawn / destroy / alloc / query / shared helpers). Systems live in the folder of the entity they mostly act on, except construction moves to builders/.

## Tasks

1. Audit (read-only): dropped, user is doing the simplification.
2. [DONE] Entity folders reorg (settlements, armies, battles, builders). Builds and runs, hand-tested OK by the user.
3. [DONE] Move construction system to builders/ (redundant `../builders/world_sim_builder.h` include left in the moved file).
4. [DONE] Imprint updates (world_sim, include_match, include_game_common).
5. [DONE] Fix bugs: `match_get_required_mem` compounding (matches never start), `army_troop_count` levies-first field order in settlement troop code. Untested: needs build + match start check.

6. [DONE] `match_get_required_mem` back to count * sizeof(::single). Untested.
7. [DONE] TS frontend adapted (ENTITY_TYPE indices, caravan rendering and Settle UI removed, `raised_*` army GDO names). Untested in browser.

## Open questions

- Settlement GDO has no levy properties (max / ceiling); TS duplicates levy max as population - 10.
- front_src.imprint.md mode tree still mentions "Settle" for Army selected; web/art/entity_caravan.svg may be stale.

- Troop pools, ceilings, raising UI: keep, move, or cut?
- Hand-test of troops/recruitment was never done.

## Remaining scope

- Economy strip-back: wealth / caravan removal done; confirm no leftovers (unused `world_sim.h` caravan declarations, stale `web/art/entity_caravan.svg`).
- Army / battle cleanup and settlement-army abstractions.
- Replacement for found settlement (entity-linked action).
- Settlement slot reuse does not reset `land_outputs`, `raised_troops`, `territories`.

## Next step

Decide the army / battle abstraction work (audit builder, read-only), and clean the leftovers above.
