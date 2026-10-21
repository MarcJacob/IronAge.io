# WORLD SIMULATION

World simulation of a match: terrain, entity lifecycle, pathing, and the systems that run every tick. Public state structures are in *include/game_common/match/world/* (see *world.h* for the world state).

## Layout

Unity build: *world_sim_main.cpp* includes every implementation file of this folder and its subfolders (that include list is the list of files), and holds world init and `world_state_tick`.
*world_sim.h* is the cross-theme API: system declarations, cross-entity functions, shared constants and inline helpers. It includes the entity headers.

- A **system** is a function called once per tick from `world_state_tick`, which holds the tick order (it matters for the result: do not reorder).
- **Entity folders**: each entity type has a subfolder (settlements, armies, battles, workers) with *world_sim_<entity>.h* and *world_sim_<entity>.cpp*.
	- The cpp holds the entity's alloc, spawn, destroy, query and commonly used functions that are not tied to a particular system.
	- The header declares them, plus constants / helpers shared by several themes.
- **Systems** are in *world_system_<theme>.cpp*, placed in the folder of the entity they mostly act on (battle system is in armies, construction in workers, territory and population in settlements).
- Files at this level: cross-entity functions (*world_entities.cpp*), world init and tick (*world_sim_main.cpp*), terrain generation (*world_terrain.cpp*), pathfinding (*world_pathing.cpp*), movement (*world_system_movement.cpp*, *world_movement.h*).
- Helpers or constants used by one theme stay in its file.
- Functions used by their own file only are `static`.
- Systems are still named `world_system_<what>`; the territory and population ones keep an older `economy` in their names.

## Adding a system

Put it in the file of its theme, in the folder of the entity it mostly acts on (new theme: new *world_system_<theme>.cpp*, included from *world_sim_main.cpp*), declare it in *world_sim.h*, call it from `world_state_tick`.

## Adding an entity type

New subfolder with *world_sim_<entity>.h* and *.cpp* (alloc, spawn, destroy, query), included from *world_sim_main.cpp*. Its state structure goes in *include/game_common/match/world/world_entities.h*.

## Simulation parameters

Global parameters ruling the simulation's dynamics belong in a single centralized structure in include/game_common/match/game_parameters.h
