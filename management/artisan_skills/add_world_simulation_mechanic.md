# ARTISAN SKILL: add a world simulation mechanic

Add a simple mechanic to the match simulation (a new rule that runs every tick, or a change
to an entity's behavior). Game common only. A command that triggers it is a separate step
(`add_match_command.md`).
Written 2026-10-18 for the current layout; the world simulation is due a reorganization
(`work_units/world_sim_reorganization_2026-10-18.md`): re-verify files and symbols against code,
and expect this skill to change.

Read first: *src/game_common/game_common.imprint.md*, *src/game_common/world_sim/world_sim.imprint.md*
(known to be incomplete), *include/game_common/match/include_match.imprint.md*.

## Where things are

- State: *include/game_common/match/world.h*: tiles / terrain and the entity containers
  (settlements, caravans, armies, builders, battles), `match_world_state`.
- Simulation: *src/game_common/world_sim/*. A mechanic is a **system**: a
  `world_system_<theme>_<what>` function called once per tick, in a fixed order, from
  `world_state_tick` (*world_sim_main.cpp*). Declarations and shared constants are in *world_sim.h*.
- Entity lifecycle (spawn / destroy / queries): *world_entities.cpp*. Systems live in their own
  file by theme: movement, combat, construction, economy (influence, population, caravans),
  pathing. Each file is included by *world_sim_main.cpp* (unity build, one translation unit).

## Rules

- **One theme per file.** A new mechanic goes in the file of its theme; a new theme gets a new
  `world_system_<theme>.cpp`, included from *world_sim_main.cpp*, declared in *world_sim.h*.
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

1. Add the state (fields in *world.h*, zero = a valid default).
2. Add the system function in the themed file; declare it in *world_sim.h*.
3. Call it from `world_state_tick` (end of the tick order unless told otherwise).
4. Report: files touched, tick position, random calls added, structs changed.

## No precedent: stop and report

- New entity types or new containers.
- Changes to the tick order of existing systems, to pathing, or to combat / economy rules the
  task does not specify.
- Balance numbers the task does not give.
- Anything touching client-read structs beyond appending a field.
