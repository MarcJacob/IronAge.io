# Basic world economy

Started: 2026-10-18 (renamed from `alpha_wealth_treasury`; absorbs `alpha_settlement_buildings` and `alpha_trade`). Design: `management/design_doc.md`, Alpha > Settlements > Local Wealth & Construction, Upkeep & Economy, Buildings, Trade.

## Scope

- Simulation time: calendar days (N ticks), daily ticks as the unit economy mechanics hook into.
- Local wealth per settlement (sources: land output, buildings, caravans), base cost per population, taxation into the owner's single treasury, treasury spending hooks. HUD display and tax control.
- Settlement buildings: definitions (effect, ticks, cost, max level) and in-settlement construction on top of the builder / build type system from `road_construction_2026-10-03.md`. Timed and financed from local wealth (no wealth = suspended). Housing (pop cap) first. Roads are instant and free today, buildings are timed and paid.
- Trade (land and sea): road network graph, caravan spawn (pop, wealth, upgrades), weighted random target selection, wealth generated at stops for home and visited settlement, caravanserai cap. Sea trade lanes included.

## Order

1. Time system (calendar days). 
2. Wealth / treasury.
3. Buildings.
4. Trade.

## Tasks

1. Time system:
   - [DONE] Survey of tick-based timing.
   - [DONE] `TICKS_PER_DAY` + `world_is_daily_tick` in `world_sim.h`; growth moved to daily tick, amounts x2; stale comments fixed. Untested: needs build + growth pace check.
2. Wealth / treasury:
   - a. Population accounting refactor (peasants / soldiers home / soldiers raised). Multi-file; also pool troops trained from peasants.
   - b. Output tick: base land output, effective output from peasants, daily wealth + population target.
   - c. Treasury + per-settlement signed tax rate + set-tax command, daily tax step.
   - d. HUD / UI for treasury and tax.
3-4. Buildings, trade: not broken down yet.

## Decisions

- Calendar day = fixed number of ticks. Mechanics are specified in days and run on a daily tick, not raw tick counts.
- Day length 100 ticks (5 s at current rate). Days only; larger units (months / years) later if needed.
- Pop and troop-pool growth move to the daily tick: same tick for all settlements (no guid stagger), same per-second rate (per-run amounts doubled).
- Battle round (20 ticks) and per-tick systems (raising draw, territory, construction, movement) stay tick-based for now.
- Tax rate: per-settlement number, stored on the settlement, resets to 0 (neutral) on change of owner. 0 = none, 100 = all wealth taxed on the next daily tick. Gameplay limit (~10) is a later input / UX matter. Player-set via command.
- Investment = negative tax rate: gifts that % of the treasury to the settlement each day. Not now, but the rate must be stored signed (range -100..100).
- Local troops == pool troops (upkeep from local wealth).
- Population accounting per settlement (struct, like army troops): peasants + soldiers per type, soldiers split between at home and in armies (raised). Levies: no home pool, only counted while in armies. Levies in the field count in the settlement population (as soldiers, not at home).
- Peasants = everyone at home who is not a soldier. Raising: peasants -> soldiers (settlement side) + levies in the army. Troop pools are trained from peasants (1 peasant -> 1 soldier of a type).
- Growth counts population at home (peasants + home soldiers) and creates new peasants.
- Land output becomes base land output = peasants that can work the land at full efficiency. Effective output = base + surplus / (1 + surplus / base), where surplus = max(0, peasants - base); below base it is just peasants (min()). Daily "output tick" runs before growth and taxes: yields base wealth and the population target. Daily order: output, growth, tax.
- Single-production / needs-consumption model (food vs commercial wealth) deferred; revisit with buildings / trade.
- Day-length constant lives in the main world_sim header, with a helper telling whether the world is currently on a daily tick.
- Sea trade stays in Alpha: only the definition of links between settlements and the pathing differ from land.
- Caravans travel only on roads (land) / established lanes (sea).

## Open questions

- Which existing periodic mechanics move to the daily tick (growth `GROWTH_TICK_PERIOD` 50, troop pool growth, battle round interval 20, raising draw)?
- Formula: effective output -> daily wealth and population target (conversion factors).
- Population shrinking above target: who dies (peasants only, soldiers too)?
- On army disband, do levies return to peasants?
- Upkeep split for armies (treasury): see `alpha_troops_recruitment`.
- Caravan return trip, or disappear at target?
- What does a caravan carry (plunder gives "carried gold")?
- How are sea lanes established?

## Current state (survey done, before simplification)

- Local wealth and caravans were removed by `world_sim_simplification`; this unit rebuilds them.
- Land output exists: terrain base output + proximity decay, summed in `land_outputs`; drives target population only.
- Cost hooks are stubs: `raise_army_can_afford` (true), `raise_army_charge` (empty), in match_commands.cpp.
- No treasury / tax / upkeep / buildings / per-player state anywhere in world state (players are only ids on entities).
- No top-level per-player HUD readout; client has no non-entity query.
- Growth: `GROWTH_TICK_PERIOD` 50, staggered by guid. Validity error flags mirrored by hand in input.ts.

## Depends on

`straight_line_movement_2026-10-15.md` (trade).

## Next step

User builds and checks growth pace. Then design wealth / treasury on the daily tick: decide the open questions, break into tasks.
