# WORLD SIMULATION

World simulation of a match: terrain, entity lifecycle, pathing, and the systems that run every tick. Public state structures are in *include/game_common/match/world/* (see *world.h* for the world state).

## Layout

Unity build: *world_sim_main.cpp* includes every implementation file of this folder and its subfolders (that include list is the list of files), and holds world init and `world_state_tick`.
*world_sim.h* is the cross-theme API: cross-entity functions, shared constants and inline helpers. It includes the system headers.

- A **system** is a function called once per tick from `world_state_tick`, which holds the tick order (it matters for the result: do not reorder).
- */entities/*: one *world_sim_entity_<entity>.h* and *.cpp* per entity type.
	- The header holds the internal symbols and helpers for the entity type (constants / helpers shared by several themes).
	- The cpp implements them, plus the public functions declared under *include/* (alloc, spawn, destroy, query, commonly used functions not tied to a particular system, entity-specific logic such as caravan destination selection or settlement building events).
- */systems/*: every system, in *world_system_<theme>.cpp* files. Declarations are in a header per category: *systems_economy.h*, *systems_war.h*, *systems_workers.h* (one function per work type), *systems_world.h* (territory and movement).
- Files at this level: cross-entity functions and world init / tick (*world_sim_main.cpp*), terrain generation (*world_terrain.cpp*), pathfinding (*world_pathing.cpp*), movement helpers (*world_movement.h*).
- Helpers or constants used by one theme stay in its file.
- Functions used by their own file only are `static`.
- Systems are still named `world_system_<what>`; the territory and population ones keep an older `economy` in their names.

## Adding a system

Put it in a *world_system_<theme>.cpp* in */systems/* (new theme: new file, included from *world_sim_main.cpp*), declare it in the matching *systems_<category>.h*, call it from `world_state_tick`.

## Adding an entity type

New *world_sim_entity_<entity>.h* and *.cpp* in */entities/* (alloc, spawn, destroy, query), the cpp included from *world_sim_main.cpp*. Its state structure goes in *include/game_common/match/world/world_entities.h*.

## Simulation parameters

Global parameters ruling the simulation's dynamics belong in a single centralized structure in include/game_common/match/game_parameters.h
