# World simulation reorganization

Started: 2026-10-18.

## Goal

Better organize the world simulation code before Alpha feature work: each system in its own implementation file, plus helper headers per theme.

## Scope

- Game common world simulation (`src/game_common/world_sim/`).
- Start from the imprints there; a good imprint pass is wanted (imprints "need a good general pass").

## Tasks

Not broken down yet, except:
- Entity GDOs (multi-file architecture change; survey and design first): a GDO for every entity type; the TS uses the GDO representation whenever it needs to view an entity; the entity view fat struct (`backend.ts` fixed-offset reader, `static_assert`ed size) may go away; the bottom-right debug view panel displays all GDO properties of the inspected entity. Decisions: entity GDOs use their own separate buffer (not the 128-byte input command one; start with one buffer holding the latest "get entity view" result); the TS lifts the data into a TS buffer with the same function used to read input command GDOs; entities have no 64-bit properties (to verify); straight entity data stays in use everywhere except at a bytes buffer / network boundary (as for commands).

## Notes

- Multi-file architecture change: surveyed and designed before any edit.

- Update `artisan_skills/add_world_simulation_mechanic.md` when the reorganization lands (layout, theme files, tick order).
- Survey done 2026-10-18 (agent report): files `world_entities`, `_system_movement`, `_system_combat`, `_system_construction`, `_system_economy` (largest, six themes), `_pathing`; undeclared non-statics, caravan despawn bypassing `world_destroy_caravan`, stale `world_state_init` declaration in `world.h`, stale imprints.

## Next step

Survey (builder, read-only): current systems, files, and shared helpers/headers; then propose a split.
