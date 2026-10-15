# WORLD SIMULATION MECHANICS

Mechanics of the world simulation, one section per system. Code is in this folder.

## Battles

Army catching an army creates a BATTLE entity (micro entity type, fixed-size state, no list of armies).

- Battle state: `men[side][unit type]`, `damage[side]` (men killed this tick BY that side), `duration`, plus siege fields `siege_settlement`, `siege_owner`, `siege_defenders`.
- Armies store `battles` (battle guid, INVALID if none) and `battle_sides`.
- Battles cannot be targeted. An army reaching a target army (`world_system_combat.cpp`) joins the side against it via `battle_join` (adds its men to the side, sets its battle / side, stops it).
  - Target not in a battle: a new battle starts, attacker side 0, target side 1.
  - Otherwise: joins the target's battle on the opposite side.
- Tick (`world_system_battles`): each side rolls damage (1-10% of its men, rounded up). Each army takes enemy damage * (army men / side men), removed cheapest unit first, and subtracted from battle `men`. Army at 0 men is destroyed.
- A side at 0 men ends the battle: survivors are released, battle destroyed.
- Armies with a battle are skipped by entity movement (pinned) and cannot leave. To the death only.
- Selecting a battle shows type, guid, location, attacker / defender men. A red / blue balance bar is drawn above the marker (`draw_battle_balance` in `render.ts`).

### Siege

- Reaching an enemy settlement without a siege raises a defending levy army (10% of population, at least 1, taken from the population) and starts a siege battle: attacker side 0, levy army (`siege_defenders`) side 1.
- Reaching an enemy settlement under siege, or targeting its defending army, joins side 0.
- Side 0 wins: settlement goes to `siege_owner`. Side 1 wins: levy army is disbanded into the settlement.

## Armies

- `world_disband_army(world, army, settlement)` destroys an army and adds its men to the settlement's population 1:1.
