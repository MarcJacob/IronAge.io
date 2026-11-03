# GAME MECHANICS (abstract, for AI development)

High-level and terse: what exists, the rules, the numbers a player cares about, what a player can do. Numbers are the current placeholders and may be tuned; update this file when mechanics change. "unverified" = not confirmed against the running game.

## Time

- A day = 120 ticks. Raid loot, economy, troop training, caravan spawn, army supplies, soldier pay and building / road financing happen once a day. The tick rate is a lobby / match setting (ticks per second; tests use 20).
- Daily effects happen in this order: raid loot -> economy (production, tax, sales, growth, waste) -> troop training -> building progress -> caravan spawn -> road / building crews -> army supplies and soldier pay. Territory, army orders, movement, army raising and engagements then run every tick.
- A new settlement runs no economy for its first 10 ticks (it claims its land first).

## Population

- Strata: unemployed, peasants, workers, soldiers. Total population = all four, soldiers being home (garrison) + raised (in armies). "At home" = everyone but raised soldiers.
- Unemployed: no job, consume goods, births land here. Their number is the pressure that claims land (see Territory). They fill free peasant jobs.
- Peasants: limited by jobs = the sum of the jobs of the owned tiles. Over the limit (land lost) they become unemployed. Only peasants produce.
- Workers: people on building projects, taken from the unemployed first, then the peasants. They consume, do not produce, and go back to the unemployed when the project ends or stalls.
- Soldiers: the home pool (men at arms, horsemen, knights, trained from the unemployed first, then peasants) and raised soldiers. Garrison levies are a separate pool that is not population, see Armies.
- Urban population (unemployed + workers + home soldiers) above the urban cap (100 + 100 per Housing) slows growth by half of the excess per day.

## Production, goods, growth

- No wealth / money per settlement: everything is *goods*, kept in two pools (see Trade): local *market goods* and *export goods*.
- Daily per settlement, in order: production -> tax -> goods added -> sales -> growth -> waste -> trade volume decay.
- Production = land surplus x min(peasants / jobs, 1) + 1 per peasant, plus the trade volume bonus (up to +75 % at full trade volume), before tax.
- Land surplus: each owned tile gives its surplus (jobs and surplus % per terrain: plains 3 jobs / 45 %, hills 2 / 30 %, mountains 1 / 30 %, coast 5 / 45 %, sea 2 / 30 %), reduced by 10 % per 30 distance units from the settlement, 0 from 300.
- Tax: the tax rate (0..10 %, `SET_SETTLEMENT_TAX_RATE`) of the production goes to the owner's *treasury* (one per player, the only treasury income). The rest is added to the goods. New start settlements 5, founded 0, conquered reset to 0.
- Capacity (both pools together) = 4 per person at home + buildings. It is only a reference (waste, caravan room, profit); growth is not tied to it and goods are never refused.
- Sales: 50 % of the LOCAL pool is offered; consumption need = 1 per person at home (raised soldiers do not consume at home). Local goods decrease by min(need, offered).
- Growth in per mille of the people at home, from offered / need: 100 % and more +2 % rising to +12 % as the excess reaches the need; 80..100 % 0..+2 %; 50..80 % 0..-20 %; 50 % or less -20 %. At least +1 if positive. Births are unemployed; deaths hit unemployed, then peasants, then workers (home soldiers never die of hunger yet).
- Waste: 25 % of the local pool above the shared capacity disappears each day, after growth. The export pool is not wasted (unverified against the design, which says waste applies to both pools).
- New settlements start with goods = 400 % of the population, unless given goods.

## Territory

- Distance-based, no contestation or pressure push. Each owned tile has a travel distance to its settlement: the shortest path over the settlement's own tiles (terrain move cost x 5 straight / 7 diagonal, roads cost 1). Roads therefore lower distance and raise surplus.
- Claiming: a settlement's border tiles claim unowned, non-sea tiles nearest-first, spending its unemployed as pressure, each tile costing its jobs. No claim beyond distance 400.
- Takeover: a tile flips to a neighbouring settlement (any owner) if its distance there is shorter by more than 25 (one straight plains step). Ties keep the owner. The taker needs unemployed left.
- Losing a tile rebuilds that settlement's distances; tiles cut off from it or past the max distance are lost too. A settlement's own tile never flips.
- Settlements can only be founded on plains / hills. Sea cannot be owned. Armies and workers cross water for now (temporary); caravans are land only.

## Settlements and colonization

- `FOUND_SETTLEMENT {origin settlement, target tile}`: takes 100 peasants (and their share of the local goods, see below), a worker walks to the tile and founds a settlement with them as unemployed. Valid target: plains / hills, tile unowned or own, at least 16 tiles from every settlement. Rechecked on arrival (else colonists walk home with their goods). Reachability is not checked when ordering.
- Colonists take local goods = local pool x colonists / total population. The export pool stays. The colony gets them through the normal goods split (its export ratio).
- Entity limits per region: 32 settlements, 64 of each of armies, workers, engagements and caravans, times the number of regions. Commands needing a slot are refused when full.

## Buildings (`BUILD_SETTLEMENT_BUILDING`)

- A project is a worker. Each day it takes its crew from the unemployed / peasants and pays its daily cost from the treasury; if either is missing the day stalls (crew released, nothing paid). No limit on parallel projects.
- Housing: 4 days, 50/day, 25 workers; +100 urban population cap. Barracks: 10 days, 100/day, 50 workers; +50 men at arms max, +2 trained per day. Caravanserai: 6 days, 100/day, 50 workers; +1 caravan slot, +300 capacity. Trade Depot: 10 days, 150/day, 50 workers; +1400 capacity.
- Troop training only happens up to the settlement's *ceiling* (`SET_SETTLEMENT_TROOP_CEILING`, 0 by default = nothing is trained) capped by the building maximums, and only while peasants stay above 10.

## Roads and workers

- `BUILD_ROAD {settlement, origin, destination}`: a crew builds a road tile by tile along a complete land path. Origin must be the settlement tile or a road tile in its territory; the destination is not checked. Crew costs 50/day from the treasury (stalls when unpaid); a tile takes 30 ticks per terrain move cost, x5 outside the player's land. Roads are the network caravans and armies prefer, and they lower territory distance.

## Armies, raising, standing orders, engagements, sieges, raiding

### Troops, raising, upkeep, supplies

- Wagons (fifth troop type, `WAGONS`): handled like levies (civilian garrison pool that stays in the working population, one pop moves into the army when raised, returns as unemployed). Max 2 per tradepost (0 without), counting garrison plus raised, pool trains `wagon_training_per_day`. No damage: an army with zero total damage cannot start or join an engagement, and a side dealing no damage in a round loses. An army containing wagons moves at half speed.
- Troop types: levies, men at arms, horsemen, knights (plus wagons, above). The last three are pooled at home (garrison) and filled by training. Levies are a garrison pool too, not population: max 20 % of the total population, trained 20 per mille of it per day (at least 1). Raising a levy kills one population (unemployed, then workers, then peasants). Garrison levies are not paid.
- `RAISE_ARMY {settlement, raise location (in its territory), target counts, start order}`: the treasury must hold the raise cost (one day of the targets' consumption, refused otherwise). The cost is taken at once and becomes the new army's supplies. The army stays still while raising: it draws up to 2 levies and 3 of each other type per tick from the garrison until targets are met. The start order is the order it gets when raising ends: defend territory of the settlement (default) or station at the raise location. `SET_ARMY_ORDER` is refused for raising armies; `STOP_RAISING_ARMY` and `SET_ARMY_RAISE_TARGETS` exist. Stopping a raise before any man joined destroys the army. The army is bound to its home settlement; raised men are not at home and do not consume there.
- Troop stats (damage, defense, armor, upkeep, carry capacity, speed) are defined in `management/design/military_design.md`, "Troop types": consult it, do not rely on numbers quoted elsewhere. Code values live in `game_parameters.h` and follow the design document.
- Upkeep per soldier per day: see the design document. Home pool soldiers (levies excepted) are paid from the treasury each day (as much as it holds, no consequence when short).
- Army supplies: every army carries a stock of goods. Each day its soldiers consume their daily upkeep amount from it. Inside territory its owner owns, the army first refills from the treasury: up to 2 days of its consumption per day, up to its carry capacity (a raising army is sized on its raise targets). Outside its owner's territory it is not supplied.
- Carry capacity = sum over troop types of count x per-man capacity (per-man capacity: see the design document). The same value caps the supplies stock and the gold stock.
- Shortage: the share of the day's consumption that supplies could not cover is the share of each troop type that deserts. Deserters go back to the home settlement or vanish if the home is gone or changed hands. An army whose last man leaves is destroyed. A raising army that runs short stops raising instead, keeping its men. No debt: the treasury covers what it can.
- `DISBAND_ARMY`: army not in an engagement and standing in its home settlement's territory; the men return home. Every disband (also the automatic ones) is a homecoming: gold goes to the owner's treasury; supplies are split, 50 % to the home settlement's market goods (if still the owner's), the rest to the treasury 1:1.

### Diplomacy and targets

- Each player has a diplomatic status towards every other player: hostile, friendly or neutral. For now every other player is hostile and nobody is friendly or neutral; the friendly and neutral concepts exist for later diplomacy. An unowned settlement counts as hostile.
- Military targets: attack and raid targets are hostile or neutral settlements; defend targets are friendly settlements (own ones for now). Attack targets can also be armies of other players and engagements. A caravan counts as hostile only if both its origin and its destination are hostile.

### Standing orders

- Each army has one standing order, set with `SET_ARMY_ORDER {army, order type, target, location}`. The AI uses it for all army movement and attacks (the older move / attack commands are only shortcuts for station / attack). An order that cannot go on falls back to STATION at the army's current location. Armies in an engagement stay in it whatever their order says (the order is kept and resumes afterwards); raising armies ignore their order.
- STATION {location}: go there and stand. Hostile armies within 5 tiles are chased while they stay within 10 tiles of the station point, then the army returns to it. Attacks resolve within 5 tiles.
- ATTACK {target}: target is a settlement, an army, or an engagement (an army that is part of an engagement sends the attacker to that engagement). The army chases the target and resolves the attack on reaching it, then stands. A settlement that is no longer hostile is defended instead. An engagement that is neutral to the owner (see Engagements) is not joined: the army waits just outside it.
- DEFEND_TERRITORY {friendly settlement}: travel to its territory, wander within 12 tiles of the settlement, attack hostile armies inside. Retreats to the settlement tile when hostile men in or near (20 tiles) the territory outnumber the army, until none is left near. It also opposes hostile sieges of its settlement.
- RAID {target settlement}: see Raiding.
- GO_HOME: walk to the home settlement and disband within 2 tiles of it, inside its territory.
- Validity of `SET_ARMY_ORDER`: own, non-raising army (the army can be anywhere for all orders). ATTACK: hostile or neutral settlement, army of another player, or any engagement. DEFEND_TERRITORY: friendly settlement. RAID: hostile or neutral settlement. GO_HOME: home settlement still the owner's. Otherwise refused.

### Engagements

- An engagement is any fight: a field battle between armies or a siege of a settlement. It runs one round every 20 ticks. Each side deals the sum of its men's damage x a random 50-100 %. The target spreads it over its men by type; each type present reduces it by a random 0 .. (men x defense); what is left kills one man per health point set (the remainder kills one more at random, proportionally). Both sides roll before anyone takes casualties. Armies reduced to nothing are destroyed.
- Per man: damage levy 2, men at arms 4, horseman 6, knight 9; defense 1, 3, 4, 7; health 20, 30, 40, 60.
- Field battle: starts halfway between two armies and lasts until one side is gone.
- Joining is automatic: a free army (not in an engagement, not raising) standing within the join radius of an engagement joins it, on the better side from its owner's viewpoint (friendly side over neutral over hostile), keeping its standing order and resuming it afterwards. Radius: 2 tiles, +1 per power of 2 of total troops above 256 (sieges: count only troops not inside the settlement).
- An engagement whose two sides rate the same to the owner (both hostile, both neutral, both friendly) is not joined: armies stay outside, close by, and wait. Armies on a besieged settlement's tile never join its defender side.
- Armies cannot withdraw from an engagement for now (`WITHDRAW_ARMY` is refused for any army in an engagement). A siege commander can still lift a siege.

### Sieges

- Attacking a hostile settlement starts or joins a siege: idle besieging, no damage rounds. A settlement with an empty garrison is taken at once. The settlement's garrison (levies included) counts as the defender side. The attacker commander is the owner of the army that started it; the defender commander is the settlement's owner.
- Phases: besieging -> assault (`BEGIN_ASSAULT`, attacker commander) -> fights until one side is gone. Sally (`SALLY_OUT`, defender commander, from besieging or outside clash): the garrison fights the besiegers. Outside clash: an army attacking the besiegers joins the defender side; the garrison and inside armies stay out; relief armies wiped = back to besieging.
- Defender side wiped = the attacker commander takes the settlement permanently (population, raised soldiers, buildings; tax resets to 0). Attacker side wiped, or no attacking army left = siege lifted.
- `LIFT_SIEGE` (attacker commander, only while besieging) ends it.
- Under siege the settlement produces nothing, consumes half, has no positive growth and spawns no caravans; caravans do not go to or from it (waiting or retargeting near it). Garrison casualties of levies kill population; survivors stay in the garrison. Training, construction and trade volume decay continue.

### Raiding

- RAID takes a target settlement chosen in the command (the army can be anywhere; it must be a hostile or neutral settlement). The army travels to its territory and raids from the moment it enters it, moving slowly (50 % speed) while it wanders inside. Stronger hostile armies within 15 tiles make it flee home until none is within 25 tiles. It falls back to STATION if the target stops being hostile.
- Daily loot (before the economy, per raiding army that is not fleeing): up to 10 % of the settlement's production (half to gold, half to supplies), 10 % of its peasants killed (1 gold each), 20 % of its stored goods, local then export (1 supply each). Each is capped by the room left in the army's gold / supplies stock and by a per-settlement daily budget shared by all raiders.
- A settlement looted on a day is raided until a day without looting: no unemployed turn into peasants, no territory growth, no takeover of land.
- Gold stock full after the loot: the order becomes GO_HOME, and the homecoming deposit applies.

## Trade

- Two goods pools per settlement: local market goods (consumed by sales, colonists) and the export pool (only caravans take from it, never consumed locally). All goods income (production after tax, caravan deliveries and refunds, colonists' goods) goes through one split by the settlement's export ratio (0..90 %, default 20 %, set with `SET_EXPORT_RATIO`). The export share is kept only up to the export cap = capacity x ratio; the rest goes local. Lowering the ratio does not move goods already in the export pool (unverified; the design notes expect it).
- Caravan slots: 1 plus 1 per Caravanserai. Caravans carry goods (conserved): origin export pool to destination pools. No goods are created; trade only creates *trade volume*. `SET_ALLOW_FOREIGN_EXPORT` restricts a settlement's caravans to own destinations.
- Spawn, daily per settlement with a free slot, at most one caravan: size ceiling = 50 % of the export pool (at least 10, at most 65535). Up to 3 picks. Candidates: every other settlement of any owner, size cut to its room (capacity minus both pools). Score = profit x 100 / tile distance; eligible if at least 50 %; pick is random, weighted by score; the chosen destination must have a complete land path, else another is picked. No reserve for the home people: starvation for a good deal is allowed.
- Profit (volume only): caravan value x (destination scarcity rate, linear in its fill from 150 % empty to 25 % full, plus a distance term), x capacity ratio factor (destination vs origin capacity, clamped to -75 % .. +100 %), x destination trade volume bonus (up to +50 %), x short-trade ramp (0 up to 8 tiles, full from 16).
- Arrival: value is added to the destination (through its split), both ends gain trade volume = value + their profit share (0 by default: the exporter gets the profit). Caravans that cannot go on pick a new destination, or walk home and refund.
- Trade volume: per settlement, capped at 2 x capacity, decays linearly by max / 30 per day. Gives production up to +75 % (at full) and more profit for caravans delivering there.

## Commands an AI can use

SET_ARMY_ORDER, FOUND_SETTLEMENT, RAISE_ARMY, BUILD_ROAD, SET_SETTLEMENT_TROOP_CEILING, SET_ARMY_RAISE_TARGETS, STOP_RAISING_ARMY, SET_SETTLEMENT_TAX_RATE, BUILD_SETTLEMENT_BUILDING, DISBAND_ARMY, SET_EXPORT_RATIO, SET_ALLOW_FOREIGN_EXPORT, BEGIN_ASSAULT, LIFT_SIEGE, WITHDRAW_ARMY (always refused for now), SALLY_OUT. (SERVER_SPAWN_PLAYER is server only.) A command can be checked for validity before emitting; an invalid one is refused and reports errors (not owner, invalid target, wrong phase, army raising, out of bounds...).

## Not implemented yet

- Diplomacy (alliances, war / neutral states, vassals, chat). Everybody is hostile to everybody.
- Player-chosen trade partners or routes (only the export ratio is player-set), plundering or blocking caravans, trade embargo.
- Other buildings of the design doc (keep, walls, trade district), building defense bonuses, army speed bonuses, sea travel (embark cost and sea consumption), negative tax (investment).
- Cross-border territory push.
- Armies: supplies outside own territory (no way to pay abroad), looting of defeated armies or conquered settlements, caravan attacks, retreat / morale, withdrawing from engagements, distance-based return of deserters.
- Victory: only a placeholder server rule (one player holding 80 % of the world population ends the match; unverified).
