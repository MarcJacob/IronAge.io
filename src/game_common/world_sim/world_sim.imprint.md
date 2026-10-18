# WORLD SIMULATION

World simulation of a match: terrain, entity lifecycle, pathing, and the systems that run every tick. Public state structures are in *include/game_common/match/world.h*.

## Layout

Unity build: *world_sim_main.cpp* includes every implementation file of this folder (that include list is the list of files), and holds world init and `world_state_tick`.
*world_sim.h* is the cross-theme API: system declarations, entity spawn / destroy, shared constants and inline helpers.

- A **system** is a function called once per tick from `world_state_tick`, which holds the tick order (it matters for the result: do not reorder).
- One implementation file per theme, named *world_system_<theme>.cpp*: territory (tile influence), population, caravans, army, battle, movement, construction.
- Other files: entity lifecycle (*world_entities.cpp*), terrain generation (*world_terrain.cpp*), pathfinding (*world_pathing.cpp*).
- Helpers or constants shared by several themes go in a *world_sim_<theme>.h* header next to the theme's file. Anything used by one theme stays in its file.
- Functions used by their own file only are `static`.
- Systems are still named `world_system_<what>`; the territory and population ones keep an older `economy` in their names.

## Adding a system

Put it in the file of its theme (new theme: new *world_system_<theme>.cpp*, included from *world_sim_main.cpp*), declare it in *world_sim.h*, call it from `world_state_tick`.
