# WORLD SIMULATION MECHANICS

Mechanics of the world simulation, one section per system. Code is in this folder.

## Battles

Army catching an army creates a BATTLE entity (micro entity type, fixed-size state, no list of armies).

- Battle state: `side_owners[2]`, `men[side][unit type]`, `damage[side]` (men killed this tick BY that side), `duration`.
- Armies store `battles` (battle guid, INVALID if none) and `battle_sides`.
- Joining: an army targets the battle like any entity; on reach (`world_system_combat.cpp`) `battle_join` adds its men to its side, sets its battle / side, stops it.
  - Reaching an army not in a battle spawns one; both join. Reaching an army in a battle joins against it.
  - Reaching a battle joins side 0 if owner matches `side_owners[0]`, else side 1.
- Tick (`world_system_battles`): each side rolls damage (1-10% of its men, rounded up). Each army takes enemy damage * (army men / side men), removed cheapest unit first, and subtracted from battle `men`. Army at 0 men is destroyed.
- A side at 0 men ends the battle: survivors are released, battle destroyed.
- Armies with a battle are skipped by entity movement (pinned) and cannot leave. To the death only.

### Siege

- An army reaching an enemy settlement spawns a defending levy army (10% of population, at least 1, taken from the population) at the settlement and starts a battle with `siege_settlement` set. Attacker is side 0, defenders side 1.
- Further attackers reaching the settlement join side 0.
- Battle ends with side 0 alive: settlement changes owner to `side_owners[0]`. Defenders win: nothing changes.
