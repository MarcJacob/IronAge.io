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

## Battles

Army catching an army creates a BATTLE entity (micro entity type, fixed-size state, no list of armies).

- Battle state: `men[side][unit type]`, `damage[side]` (men killed this tick BY that side), `duration`, plus siege fields `siege_settlement`, `siege_owner`, `siege_defenders`.
- Armies store `battles` (battle guid, INVALID if none) and `battle_sides`.
- Battles cannot be targeted. An army reaching a target army (*world_system_army.cpp*) joins the side against it via `battle_join` (*world_system_battle.cpp*; adds its men to the side, sets its battle / side, stops it).
  - Target not in a battle: a new battle starts, attacker side 0, target side 1.
  - Otherwise: joins the target's battle on the opposite side.
- Battles only run a round every `BATTLE_TICK_INTERVAL` (10) world ticks (`world.age % interval == 0`); `duration` counts rounds.
- Round (`world_system_battles`): each side rolls damage (1-10% of its men, rounded up). Each army takes enemy damage * (army men / side men), removed cheapest unit first, and subtracted from battle `men`. Army at 0 men is destroyed.
- A side at 0 men ends the battle: survivors are released, battle destroyed.
- Battle location is fixed at spawn: midpoint of the two starting armies (attacker and settlement for a siege). It never moves.
- Armies in a battle walk to the battle location through normal movement; within `BATTLE_ARMY_DISTANCE` (2 tiles) they are pinned. They cannot leave (move orders ignored). To the death only.
- Selecting a battle shows type, guid, location, attacker / defender men. A red / blue balance bar is drawn above the marker (`draw_battle_balance` in `render.ts`).

### Siege

- Reaching an enemy settlement without a siege raises a defending levy army (10% of population, at least 1, taken from the population) and starts a siege battle: attacker side 0, levy army (`siege_defenders`) side 1.
- Reaching an enemy settlement under siege, or targeting its defending army, joins side 0.
- Side 0 wins: settlement goes to `siege_owner`. Side 1 wins: levy army is disbanded into the settlement.
- Siege code is currently split between the army file (start, join) and the battle file (outcome).

## Armies

- `world_disband_army(world, army, settlement)` destroys an army and adds its men to the settlement's population 1:1.
