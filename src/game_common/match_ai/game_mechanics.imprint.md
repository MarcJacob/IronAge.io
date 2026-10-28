# GAME MECHANICS (as of the code, for AI development)

High-level and terse. Rewritten from the code on 2026-10-29; update it when mechanics change. Numbers are the placeholders of *game_parameters.h*, the building table in *world_entity_settlements.h* and the world sim. "unverified" = not confirmed against the running game.

## Time

- A day = `ticks_per_day` (120) ticks. Economy, troop training, caravan spawn, army supplies, soldier pay and building / road financing run on the first tick of each day. The tick rate is a lobby / match parameter (ticks per second; tests use 20).
- Tick order: economy -> troop training -> (building daily hooks, none used) -> caravan spawn -> workers -> army supplies -> pool soldier pay -> territory -> movement -> caravan arrival -> armies / battles.
- A new settlement runs no economy for its first 10 ticks (it claims its land first).

## Population

- Strata: unemployed, peasants, workers, soldiers. `pop_total` = all four, soldiers being home (garrison) + raised (in armies). "At home" = everyone but raised soldiers.
- Unemployed: no job, consume goods, births land here. Their number is the pressure that claims land (see Territory). Each tick they fill free peasant jobs.
- Peasants: limited by jobs = the sum of the jobs of the owned tiles (`pop_peasants_cap`). Over the cap (land lost) they become unemployed. Only peasants produce.
- Workers: people on building projects, taken from the unemployed first, then the peasants. They consume, do not produce, and go back to the unemployed when the project ends or stalls.
- Soldiers: the home pool (men at arms, horsemen, knights, trained from the unemployed first, then peasants) and raised soldiers. Levies are meant to be taken from the peasants, see Armies.
- Urban population (unemployed + workers + home soldiers) above `pop_urban_cap` (100 + 100 per Housing) slows growth by half of the excess per day.

## Production, goods, growth

- No wealth / money per settlement: everything is *goods*, kept in two pools (see Trade): local *market goods* and *export goods*.
- Daily per settlement, in order: production -> tax -> goods added -> sales -> growth -> waste -> trade volume decay.
- Production = land surplus x min(peasants / jobs, 1) + 1 per peasant, plus the trade volume bonus (up to `production_bonus_percent_max` at full trade volume), before tax.
- Land surplus: each owned tile gives its surplus (table `TILE_TERRAIN_ARABLE_LAND`: jobs and surplus % per terrain), reduced by 10 % per 30 distance units from the settlement, 0 from 300.
- Tax: `tax_rate` (0..10 %, `SET_SETTLEMENT_TAX_RATE`) of the production goes to the owner's *treasury* (one per player, the only treasury income). The rest is added to the goods. New start settlements 5, founded 0, conquered reset to 0.
- Capacity (both pools together) = 4 per person at home + buildings. It is only a reference (waste, growth is not tied to it, caravan room, profit); goods are never refused.
- Sales: `sellable_percent` (50) of the LOCAL pool is offered; consumption need = 1 per person at home (raised soldiers do not consume at home). Local goods decrease by min(need, offered).
- Growth in per mille of the people at home, from offered / need: 100 % and more +2 % rising to +12 % as the excess reaches the need; 80..100 % 0..+2 %; 50..80 % 0..-20 %; 50 % or less -20 %. At least +1 if positive. Births are unemployed; deaths hit unemployed, then peasants, then workers (home soldiers never die yet).
- Waste: `overflow_waste_percent` (25) of the local pool above the shared capacity disappears each day, after growth. The export pool is not wasted (unverified against the design, which says waste applies to both pools).
- New settlements start with goods = `start_goods_percent` (400) % of the population, unless given goods.

## Territory

- Distance-based, no contestation or pressure push. Each owned tile has a travel distance to its settlement: the shortest path over the settlement's own tiles (terrain move cost x 5 straight / 7 diagonal, roads cost 1). Roads therefore lower distance and raise surplus.
- Claiming: a settlement's border tiles claim unowned, non-sea tiles nearest-first (16 border candidates per tick), spending its unemployed as pressure, each tile costing its jobs. No claim beyond distance `TERRITORY_MAX_DISTANCE` (400).
- Takeover: a tile flips to a neighbouring settlement (any owner, players ignored) if its distance there is shorter by more than 25 (one straight plains step). Ties keep the owner. The taker needs unemployed left. Staggered per tick (4 settlements, 128 decisions).
- Losing a tile rebuilds that settlement's distances; tiles cut off from it or past the max distance are lost too. A settlement's own tile never flips.
- Settlements can only be founded on plains / hills. Sea cannot be owned. Armies and workers cross water for now (`armies_workers_cross_water`, temporary); caravans are land only. A tile owner is a settlement slot.

## Settlements and colonization

- `FOUND_SETTLEMENT {origin settlement, target tile}`: takes `found_settlement_colonists` (100) peasants (and their share of the local goods, see below), a worker walks to the tile and founds a settlement with them as unemployed. Valid target: plains / hills, tile unowned or own, at least 16 tiles from every settlement. Rechecked on arrival (else colonists walk home with their goods). No reachability check when ordering.
- Colonists take local goods = local pool x colonists / total population. The export pool stays. The colony gets them through the normal goods split (its export ratio).
- Entity limits per region: 32 settlements, 64 of each micro type (armies, workers, battles, caravans), times the number of regions. Commands needing a slot are refused when full.

## Buildings (`SETTLEMENT_BUILDING_TABLE`, `BUILD_SETTLEMENT_BUILDING`)

- A project is a worker. Each day it takes its crew from the unemployed / peasants and pays `cost_per_day` from the treasury; if either is missing the day stalls (crew released, nothing paid). No limit on parallel projects.
- Housing: 4 days, 50/day, 25 workers; +100 urban population cap. Barracks: 10 days, 100/day, 50 workers; +50 men at arms max, +2 trained per day. Caravanserai: 6 days, 100/day, 50 workers; +1 caravan slot, +300 capacity. Trade Depot: 10 days, 150/day, 50 workers; +1400 capacity.
- Troop training only happens up to the settlement's *ceiling* (`SET_SETTLEMENT_TROOP_CEILING`, raw value 0 by default = nothing is trained) capped by the building maximums, and only while peasants stay above `raise_min_population_left` (10).
- Buildings are addressed by table row: reordering the table breaks anything holding a raw row index. The match AI looks rows up by name (see *match_ai_internal.h*), so renaming a row means renaming it there too.

## Roads and workers

- `BUILD_ROAD {settlement, origin, destination}`: a crew builds a road tile by tile along a complete land path. Origin must be the settlement tile or a road tile in its territory; the destination is not checked. Crew costs 50/day from the treasury (stalls when unpaid); a tile takes 30 ticks per terrain move cost, x5 outside the player's land. Roads are the network caravans and armies prefer, and they lower territory distance.

## Armies, raising, battles, sieges

- Troops: levies (meant to come from peasants), men at arms, horsemen, knights (pooled at home, filled by training). Battle strength counts men only: troop type does not matter yet.
- `RAISE_ARMY {settlement, raise_location (in its territory), target counts}`: levy target + 10 must be available as peasants (checked, nothing deducted), and the treasury must hold the raise cost (one day of the targets' consumption, refused otherwise). The cost is taken at once and becomes the new army's supplies. The army stays still while raising: levies 2 per tick, pooled types 3 per tick, until targets are met. Raising armies refuse move / attack orders (`STOP_RAISING_ARMY`, `SET_ARMY_RAISE_TARGETS` exist). Stopping a raise before any man joined destroys the army. The army is bound to its home settlement; raised men are not at home and do not consume there.
- Known gap: levies are drawn from the home pool (`garrison.levies`), which stays empty, so levy targets never fill and raising never ends by itself; disbanding returns levies to that pool too, not to the peasants (read from the code, unverified in play).
- Per soldier per day rates (`upkeep_*`): levy 0.5, men at arms 2, horseman 10, knight 30. Home pool soldiers are paid from the treasury each day (as much as it holds, no consequence when short).
- Army supplies: every army carries a stock of goods. Each day its soldiers consume their daily rates from it. Inside territory its owner owns, the army first refills from the treasury: up to 2 days of its consumption per day, up to a capacity of 10 days (`army_supply_capacity_days`, `army_supply_refill_days`; a raising army is sized on its raise targets). Outside its owner's territory it is not supplied.
- Shortage: the share of the day's consumption that supplies could not cover is the share of each troop type that deserts. Deserters go back to the home settlement or vanish if the home is gone or changed hands. An army whose last man leaves is destroyed. A raising army that runs short stops raising instead, keeping its men. No debt: the treasury covers what it can.
- `SET_ENTITY_MOVE_TARGET` walks an army. `SET_ENTITY_ATTACK_TARGET {army, settlement or army of another player}` chases it; within 5 tiles the attack resolves. Armies never engage by themselves.
- Battle: rounds every 20 ticks; each side loses a random 1-10 % of its men per round (rounded up; the bigger side wins, no unit bonuses yet). Armies can join a battle by attacking either side.
- Siege: attacking a settlement opens a siege battle against nobody: no defenders are raised yet (no garrison or levy defence), so at the next battle round the attackers take the settlement with its population, raised soldiers and buildings; tax resets to 0. Settlements keep no garrison otherwise.
- `DISBAND_ARMY`: army not in a battle and standing in its home settlement's territory; the men return home.

## Trade

- Two goods pools per settlement: local market goods (consumed by sales, colonists) and the export pool (only caravans take from it, never consumed locally). All goods income (production after tax, caravan deliveries and refunds, colonists' goods) goes through one split by the settlement's `export_ratio` (0..`export_ratio_max_percent` 90 %, default 20 %, set with `SET_EXPORT_RATIO`). The export share is kept only up to the export cap = capacity x ratio; the rest goes local. Lowering the ratio does not move goods already in the export pool (no such code found; the design notes expect it).
- Caravan slots: `start_caravan_slots` (1) plus 1 per Caravanserai. Caravans carry goods (conserved): origin export pool to destination pools. No goods are created; trade only creates *trade volume*.
- Spawn, daily per settlement with a free slot, at most one caravan: size ceiling = `max_caravan_export_pool_percent` (50) % of the export pool (at least 10, at most 65535). Up to 3 picks. Candidates: every other settlement of any owner, size cut to its room (capacity minus both pools). Score = profit x 100 / tile distance; eligible if at least `min_profit_per_travel_dist_percent`; pick is random, weighted by score; the chosen destination must have a complete land path, else another is picked. No reserve for the home people: starvation for a good deal is allowed.
- Profit (volume only): caravan value x (destination scarcity rate, linear in its fill, plus a distance term), x capacity ratio factor (destination vs origin capacity, clamped), x destination trade volume bonus (up to +50 %), x short-trade ramp (0 up to 8 tiles, full from 16). Parameters in *game_parameters.h*, formula in *world_trade.h*.
- Arrival: value is added to the destination (through its split), both ends gain trade volume = value + their profit share (`profit_receiver_percent`, 0 = exporter gets the profit). Caravans that cannot go on pick a new destination, or walk home and refund.
- Trade volume: per settlement, capped at `volume_max_multiplier` (2) x capacity, decays linearly by max / `volume_decay_days` (30) per day. Gives production up to +75 % (at full) and more profit for caravans delivering there.

## Commands an AI can use

SET_ENTITY_MOVE_TARGET, SET_ENTITY_ATTACK_TARGET, FOUND_SETTLEMENT, RAISE_ARMY, BUILD_ROAD, SET_SETTLEMENT_TROOP_CEILING, SET_ARMY_RAISE_TARGETS, STOP_RAISING_ARMY, SET_SETTLEMENT_TAX_RATE, BUILD_SETTLEMENT_BUILDING, DISBAND_ARMY, SET_EXPORT_RATIO. (SERVER_SPAWN_PLAYER is server only.) Each has a validity check usable before emitting (`match_command_validity_check`).

## Not implemented yet

- Diplomacy (alliances, war / neutral states, vassals, chat). Everybody can attack everybody.
- Player-chosen trade partners or routes (only the export ratio is player-set), plundering or blocking caravans, trade embargo.
- Other buildings of the design doc (keep, walls, trade district), defense bonuses, troop type combat bonuses, army speed bonuses, sea travel (embark cost and sea consumption), negative tax (investment).
- Siege defence (garrison or levies fighting for the settlement), cross-border territory push.
- Armies: supplies outside own territory (no way to pay abroad), homecoming of the supplies stock, looting of supplies from defeated armies or settlements, distance-based return of deserters.
- Victory: only a placeholder server rule (one player holding 80 % of the world population ends the match; unverified).
