# ARTISAN SKILL: add a world simulation mechanic

Add a simple mechanic to the match simulation (a new rule that runs every tick, or a change
to an entity's behavior). Game common only. A command that triggers it is a separate step
(`add_match_command.md`).
Written 2026-10-18 for the reorganized layout (`work_units/world_sim_reorganization_2026-10-18.md`).

Read first: *src/game_common/game_common.imprint.md*, *src/game_common/world_sim/world_sim.imprint.md*,
*include/game_common/match/include_match.imprint.md*.

## Where things are

- State: *include/game_common/match/world.h*: tiles / terrain and the entity containers
  (settlements, caravans, armies, builders, battles), `match_world_state`.
- Simulation: *src/game_common/world_sim/*. A mechanic is a **system**: a
  `world_system_<what>` function called once per tick, in a fixed order, from
  `world_state_tick` (*world_sim_main.cpp*). Declarations and shared constants are in *world_sim.h*.
- Entity lifecycle (spawn / destroy / queries): *world_entities.cpp*. Systems live in their own
  file by theme: *world_system_<theme>.cpp* for territory, population, caravans, army, battle,
  movement, construction. Also terrain generation (*world_terrain.cpp*) and pathing (*world_pathing.cpp*).
  Each file is included by *world_sim_main.cpp* (unity build, one translation unit).
- Code shared by several themes goes in a *world_sim_<theme>.h* next to them (e.g. *world_sim_army.h*);
  the cross-theme API stays in *world_sim.h*.

## Rules

- **One theme per file.** A new mechanic goes in the file of its theme; a new theme gets a new
  `world_system_<theme>.cpp`, included from *world_sim_main.cpp*, its systems declared in *world_sim.h*.
  Do not grow an unrelated file. Do not mix themes in one function.
- Only touch the state that mechanic needs; other systems' state through their helper / spawn /
  destroy functions, not by editing their arrays (use `world_destroy_*`, `world_spawn_*`).
- Determinism (lockstep, same result on every machine and on the client):
  - The code is built with strict float flags (no fast-math, no contraction) so native and wasm
    agree: do math the way the surrounding code does, no platform-dependent functions.
  - Randomness only via `world.rand_gen`, never a new generator. The
    call order is part of the result: adding a random call, or moving a system in the tick
    order, changes every match. Put a new system at the end of the tick unless the task says
    otherwise.
  - Iterate entity arrays in slot order.
- No standard library; `std_types.h` integer types; snake_case; `constexpr` for balance numbers
  at the top of the file; comments `//`.
- Some structs in *world.h* (entity data) are read by the client, in C++ and by the TS at fixed
  byte offsets (*backend.ts*; sizes are `static_assert`ed). Do not move, resize or remove existing
  fields of these: that silently shifts the offsets. Adding a field means checking those readers
  and listing it in the report.
- New state needs memory: match memory is sized from the `sizeof` of the `::single` structs
  (`match_get_required_mem`); report any large addition.
- A mechanic that needs a player action needs a command: stop and use `add_match_command.md`.

## Steps

1. Add the state (fields in *world.h*, zero = a valid default). Every entity type has a Game Data
   Object right below its container: add a `GAME_DATA_PROPERTY` line for the new field so the
   client can read it (the debug panel and the front end read entities only through it;
   `GAME_DATA_PROPERTY_PATH` with a flat name for fields of nested structs). Entity structs are
   NOT packed (only command structs are); their GDO is defined with the same
   `GAME_DATA_OBJECT_DEF_BEGIN` as any other. A new entity type needs its single, a `using` alias
   `world_entity_<type>`, its GDO and one case in `game_client_query_entity_gdo`.
2. Add the system function in the themed file; declare it in *world_sim.h*.
3. Call it from `world_state_tick` (end of the tick order unless told otherwise).
4. Report: files touched, tick position, random calls added, structs changed.

## No precedent: stop and report

- New entity types or new containers.
- Changes to the tick order of existing systems, to pathing, or to combat / economy rules the
  task does not specify.
- Balance numbers the task does not give.
- Anything touching client-read structs beyond appending a field.
