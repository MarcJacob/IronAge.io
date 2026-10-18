# World simulation reorganization

Started: 2026-10-18.

## Goal

Better organize the world simulation code before Alpha feature work: each system in its own implementation file, plus helper headers per theme.

## Scope

- Game common world simulation (`src/game_common/world_sim/`).
- Start from the imprints there; a good imprint pass is wanted (imprints "need a good general pass").

## Tasks

1. [DONE, user to build / verify] File split by theme: `world_system_territory/population/caravans/army/battle.cpp`, `world_terrain.cpp`, helper headers `world_sim_population/army/battle.h`; economy and combat files removed; siege file later. Cleanups done (undeclared non-statics made static, caravan despawn via `world_destroy_caravan`, stale `world_state_init` declaration removed, terrain generation extracted). Imprints (`world_sim`, `game_common`, `include_game_common`) and `add_world_simulation_mechanic.md` updated.
2. Dropped: rename of `world_system_economy_*` (territory / population functions); the imprint notes the older naming.

Unit closed 2026-10-18 (all verified in game).
3. [DONE, user to build / verify] Entity GDOs for all types (settlement, army, caravan, battle, builder minus `action_data`; battle arrays flattened `men_side<N>_*`); all consumers moved to `query_entity` / `EntityData` (`gdo.ts`); `entity_full_view` and its buffer / export / TS reader deleted; entity GDO buffer 256 bytes (largest: battle 231). No front end inspection content in `world_sim.imprint.md` (user decision). User verified in game. Original slice 1 notes (settlement): packed settlement single + `land_influence`, `gdo_world_settlement` in `world.h`, `GAME_DATA_PROPERTY_PATH` macro, `game_client_query_entity_gdo` (one case per type), separate 256-byte entity GDO buffer + 2 wasm exports, `query_entity_properties` in TS (schema cached per entity type), debug panel lists all GDO properties (other types still use the fat view). Next: army, caravan, battle (flatten arrays; add minimal single), then move consumers off the fat struct (modes, `render.ts` battle bar, `is_own_*`), delete the fat struct. Builders later.
- Survey findings: structs in `world.h` are unpacked (GDO macro needs `alignof == 1`; builder `action_data` has `alignas(4)`); GDO header lacks nested paths, fixed arrays and `::` names; fat `entity_full_view` (38 bytes, `game_client_query_entity`, `backend.ts` fixed offsets); schema must be cached per entity type for per-frame callers; proposed first slice: settlement end to end (leaf properties only), then army / caravan / builder / battle, then consumers, delete fat struct last.
- Update: entity packing undone (entity GDOs never leave the wasm instance); one `GAME_DATA_OBJECT_DEF_BEGIN` macro accepts any struct alignment, command structs keep their own `pack(1)`; aliases are `world_entity_*` with `gdo_world_entity_*`; GDO sizes now: battle 233, army 182, settlement 138, caravan 112, builder 67 (buffer 256).
- Entity GDO decisions (user; the packing decision below was superseded): add a path-based GDO property macro (name + member path, only a few lines); properties stay a flat array with flat names; battle arrays flattened (no array support yet); builder `action_data` left out until a byte-buffer property exists (sub-GDO by build type later); pack singles and nested structs (incl. `world_build_action_data`); no guid / type in the GDO (consumers know them); GDO definitions top level in `world.h` next to the singles, with `using` aliases for `::single` names; battle gets a minimal single later; raw units and C++ names in the panel; schema cached per entity type, data kept current by re-querying; builders not inspectable for now, but inspection works for any entity type that has a GDO; first slice = settlement end to end.
- Entity GDOs (multi-file architecture change; survey and design first): a GDO for every entity type; the TS uses the GDO representation whenever it needs to view an entity; the entity view fat struct (`backend.ts` fixed-offset reader, `static_assert`ed size) may go away; the bottom-right debug view panel displays all GDO properties of the inspected entity. Decisions: entity GDOs use their own separate buffer (not the 128-byte input command one; start with one buffer holding the latest "get entity view" result); the TS lifts the data into a TS buffer with the same function used to read input command GDOs; entities have no 64-bit properties (to verify); straight entity data stays in use everywhere except at a bytes buffer / network boundary (as for commands).

## Notes

- Multi-file architecture change: surveyed and designed before any edit.

- Update `artisan_skills/add_world_simulation_mechanic.md` when the reorganization lands (layout, theme files, tick order).
- Survey done 2026-10-18 (agent report): files `world_entities`, `_system_movement`, `_system_combat`, `_system_construction`, `_system_economy` (largest, six themes), `_pathing`; undeclared non-statics, caravan despawn bypassing `world_destroy_caravan`, stale `world_state_init` declaration in `world.h`, stale imprints.

## Next step

Survey (builder, read-only): current systems, files, and shared helpers/headers; then propose a split.
