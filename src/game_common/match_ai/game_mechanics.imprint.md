# GAME MECHANICS (as of the code, for AI development)

High-level and terse. Written from the code on 2026-10-22; update it when mechanics change. Numbers are the placeholders of `game_parameters.h` and the world sim.

## Time

- A day = `ticks_per_day` (100) ticks. Economy, troop training, upkeep and building financing run on the first tick of each day. Test/server tick rate: 20 ticks per second.

## Wealth, tax, treasury

- Each settlement has *local wealth*. Daily: peasants working the land produce income (peasants up to the settlement's `base_land_output` at full efficiency, extra peasants give diminishing returns, +6%); 10% of it goes to a trade pool only if a Caravanserai gave pool room.
- Tax: `tax_rate` (0..10 %, `SET_SETTLEMENT_TAX_RATE`) of the local wealth moves to the owner's *treasury* (one per player). New settlements start at 0 (founded) / 5 (start), conquered ones reset to 0.
- Consumption: 1 wealth per person at home (peasants, workers, home soldiers; raised soldiers do not consume). Taxing takes from what feeds the people: high tax = slower growth.
- Treasury pays: construction (buildings 100/day, road crews 50/day) and army upkeep (per day: levy 0.5, men at arms 2, horseman 10, knight 30). It never goes below 0 and unpaid upkeep has no penalty yet. Raising an army costs nothing.

## Population and growth

- Settlement population = peasants + workers (on a construction project) + home soldiers + raised soldiers. Only free peasants work.
- Daily growth in per mille of the people at home from how well consumption was met: about +4 % to +10 % when met, 0 at 90 % met, down to -20 % at 50 % or less. Stored as `growth_per_mille` (last day).

## Territory

- A settlement claims nearby tiles while its pressure (population - 2 x tiles owned) is positive, nearest first, and can push into foreign tiles if its pressure ratio beats the owner's. Tile output: plains 40, hills 28, mountains 15, coast 100, sea 20 (decreasing with distance to the settlement). A tile owner is a settlement slot.
- Terrain: plains / hills are buildable; mountains / water are not (armies and workers are land only).

## Settlements and colonization

- `FOUND_SETTLEMENT {origin settlement, target tile}`: takes `found_settlement_colonists` (150) peasants, a worker walks to the tile and founds a settlement with them as population. Valid target: plains / hills, tile unowned or own, at least 16 tiles from every settlement (`world_is_valid_settlement_location`). Rechecked on arrival (else colonists walk home). No reachability check: an unreachable site leaves the worker stuck.
- Entity limits per region: 10 settlements, 32 of each micro type (armies, workers, battles, caravans). Commands needing a slot are refused when full.

## Buildings (`SETTLEMENT_BUILDING_TABLE`, `BUILD_SETTLEMENT_BUILDING`)

- A project is a worker. Each day it takes its workers from the peasants and pays `cost_per_day` from the treasury; if either is missing the day stalls (no progress, no payment). No limit on parallel projects.
- Barracks (row 0): 5 days, 100/day, 10 workers. +50 men at arms max, +2 men at arms trained per day. Caravanserai (row 1): 10 days, 100/day, 25 workers. +1 max caravan, +200 trade pool cap.
- Troop training only happens up to the settlement's *ceiling* (`SET_SETTLEMENT_TROOP_CEILING`, raw value 0 by default = nothing is trained) capped by the building maximums.

## Roads and workers

- `BUILD_ROAD {settlement, origin, destination}`: a crew builds a road tile by tile along the path, from the settlement tile or an own road tile in its territory. Crew costs 50/day from the treasury (stalls when unpaid); a tile takes 30 ticks per terrain move cost, x5 outside the player's land. Validity only checks the settlement and map bounds. Roads are the network caravans use; they lower travel cost.

## Armies, raising, battles, sieges

- Troops: levies (from peasants, no home pool), men at arms, horsemen, knights (pooled at home, filled by training). Battle strength currently counts men only: troop type does not matter yet.
- `RAISE_ARMY {settlement, raise_location (in its territory), target counts}`: levy target + 10 must be available as peasants. The new army stays still while raising: levies 1 per tick, pooled types 3 per tick, until targets are met. Raising armies refuse move / attack orders (`STOP_RAISING_ARMY`, `SET_ARMY_RAISE_TARGETS` exist). The army is bound to its home settlement; raised men are not at home and do not consume.
- `SET_ENTITY_MOVE_TARGET` walks an army (land pathing). `SET_ENTITY_ATTACK_TARGET {army, settlement or army of another player}` chases it; within 5 tiles the attack resolves. Armies never engage by themselves.
- Battle: rounds every 20 ticks; each side loses a random 1-10 % of its men per round (so the bigger side wins, no unit bonuses yet). Armies can join a battle by attacking either side.
- Siege: attacking a settlement raises its defenders: 20 % of its population as levies (taken from peasants) plus its whole home pool. Attackers wiping them take the settlement with its population (raised soldiers included); defenders winning disband back into it. Settlements keep no garrison otherwise.
- `DISBAND_ARMY`: army not in a battle and standing in its home settlement's territory; the men return home (levies become peasants).

## Commands an AI can use

SET_ENTITY_MOVE_TARGET, SET_ENTITY_ATTACK_TARGET, FOUND_SETTLEMENT, RAISE_ARMY, BUILD_ROAD, SET_SETTLEMENT_TROOP_CEILING, SET_ARMY_RAISE_TARGETS, STOP_RAISING_ARMY, SET_SETTLEMENT_TAX_RATE, BUILD_SETTLEMENT_BUILDING, DISBAND_ARMY. (SERVER_SPAWN_PLAYER is server only.) Each has a validity check usable before emitting (`match_command_validity_check`).

## Not implemented yet

- Diplomacy (alliances, war / neutral states, vassals, chat). Everybody can attack everybody.
- Player-controlled trade: caravans spawn automatically from Caravanserai pools; no trade commands, no plundering.
- Other buildings of the design doc (housing, keep, walls, trade district), defense bonuses, troop type combat bonuses, army speed bonuses, sea travel, upkeep penalties, negative tax (investment).
- Victory: only a placeholder server rule (one player holding 80 % of the world population ends the match).
