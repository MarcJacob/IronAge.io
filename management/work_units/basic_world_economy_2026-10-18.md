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
   - a. Population accounting refactor (peasants / soldiers home / soldiers raised, `settlement_population`). Coded incl. casualties mirrored to home settlement (`world_army_take_casualties`) and pool growth +2/day. Untested: needs build + hand checks (raise, disband, pool growth, display, siege, battle casualties).
   - b. [DONE, untested] Daily economy tick: base land output, effective output, wealth, tax, consumption, growth. Per-player `world.player_states[player]` (`world_player_state`: treasury, income, expenses, net_change; GDO below the struct; deposit-only treasury), `tax_rates` signed, `settlement_finances`; GDO: `base_land_output`, `local_wealth`, `tax_rate`, `finance_*`. Territory `pressure` kept (used by expansion). To check: spawn starvation (500 peasants vs one tile), stockpile behavior.
   - c. [DONE, untested] Set-tax command `SET_SETTLEMENT_TAX_RATE {settlement, tax_rate}`: owner only, within `tax_rate_min` 0 / `tax_rate_max` 10 (game parameters; negative = future investment). Slider next to local wealth in the settlement inspector, negative blue / positive yellow, owned only.
   - d. [DONE, untested] UI: treasury readout, calendar, wealth / population rows (see task B notes).
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
- Disband: levies -> peasants; other types rejoin the local troops (raised count of each type goes down by the army's amount).
- Population shrink: total deaths computed, then split peasants / soldiers. For now peasants only. Later: soldier share of deaths (a %), spread over home soldiers, ordered by type (knights last).
- Siege defense left as is for now (to revisit with military mechanics); only minimally adapted to compile with the new struct.
- Growth counts population at home (peasants + home soldiers) and creates new peasants.
- Land output becomes base land output = peasants that can work the land at full efficiency. Effective output = base + surplus / (1 + surplus / base), where surplus = max(0, peasants - base); below base it is just peasants (min()).
- Daily order: (1) output: effective output -> wealth via a small function (1 wealth per unit now; multipliers / bonuses later), added to local wealth; (2) tax % of local wealth moves to treasury; (3) consumption: base consumption = f(total population at home) is deducted from local wealth; (4) growth from how well consumption was met.
- No target population anymore: growth depends only on consumption vs current population.
- Growth rule (r = wealth available / required consumption): enough: +2%, plus up to +2% more scaled by excess / consumption (capped at excess == consumption; excess is not consumed). Shortfall: growth goes from the enough-level down to 0% at 10% under required; then shrinks, reaching -20% at 50% of required; no worse below that.
- Every transaction to / from local wealth and treasury must be individually determinable (clients need an accurate breakdown). Likely a financial state / ledger object; not designed yet.
- Growth curve: r >= 1 -> +2% (+ up to +2% by excess / consumption), 0% at r = 0.9, -20% at r = 0.5 and below; linear between. "Available" = whole local wealth after tax, stockpile included. No food vs wealth distinction for now.
- Consumption: 1 wealth per person at home per day. Small-population percentages get sensible minimums (min +1 when growing, min -1 when shrinking).
- Financial breakdown: per-settlement struct of last day's figures (income, tax, consumption, excess, growth rate) written by the daily tick, exposed via GDO.
- Battle casualties: deducted from the army AND from the home settlement (raised counts and total), freeing room in pools / peasants.
- Single-production / needs-consumption model (food vs commercial wealth) deferred; revisit with buildings / trade.
- Day-length constant lives in the main world_sim header, with a helper telling whether the world is currently on a daily tick.
- Sea trade stays in Alpha: only the definition of links between settlements and the pathing differ from land.
- Caravans travel only on roads (land) / established lanes (sea).

## Open questions

- Which existing periodic mechanics move to the daily tick (growth `GROWTH_TICK_PERIOD` 50, troop pool growth, battle round interval 20, raising draw)?
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

State (2026-10-20): time system, accounting, daily tick, game parameters, inspection feature, tax command, player state panel, ceiling change: all coded. User hand-tested the first batch OK; since then untested: ceiling change (quirk: slider range 0..max, marker can pass the end if home + raised > max), GDO buffer fix (384 -> 1024, static_asserts per entity GDO; command GDO buffer is 128 bytes, largest command 82), `world_player_state` + income / deficit next to gold.

Decided along the way: army gauges show current vs raised count as max (no "(X Raised)"); `GAME_PARAMETERS` stays where it is; ceiling targets home + raised (training room = min(ceiling, max) - (home + raised); lowering discards only home troops down to ceiling - raised; `ceiling_*` GDO = effective total ceiling); enemy info is not hidden in inspection panels; remaining constants sweep (territory, battle, movement, army reach, terrain) happens with the systems they belong to.

Housekeeping for the user: delete stale compiled .js in web/src (ui/ui_element_treasury.js, troop_gauges.js, ui_element_hint.js, ui_element_properties_panel.js, mode_hint_ui.js, settlement_bar_ui.js).

UI walkthrough done (user caught up). User added `settlement_count`, `total_population`, `total_troops` to `world_player_state` with ROWS entries, tested OK; peer added `soldiers_raised()` / `soldiers_total()` to `settlement_population`. Player totals are updated inline at ~10 sites (settlement spawn / destroy / owner change, siege levies, disband, casualties, found settlement, ceiling lowering).

Decided: both. Primitive inlined helpers per entity in `world_sim_<entity>.h` (world + index, every mutation of population / troops / owner goes through them; they maintain settlement invariants, not player stats); `world_update_player_stats` in world_sim_main.cpp after each tick recomputes settlement_count, total_population, total_troops. Coded, untested (helpers in `world_sim_army.h` / `world_sim_settlement.h`, `TROOP_TYPE_*`; stats lag one tick; disband returns exactly the released amount). Then design buildings (housing vs consumption-based growth, no target pop) and trade.

## UI wishes (user, 2026-10-20)

- Generic panel design (experimental, must be tidy and easy to remove / replace): panel = title + ordered rows; row types are reusable elements (gauge, value row with delta + optional hover breakdown, text row); per-entity-type inspector functions compose shared section builders (troops, population, wealth); one generic tooltip element. Order: A (container + inspectors, no visible change), B (value row, tooltip, wealth / population sections), C (treasury HUD, needs client query of per-player state).
- Every selected entity gets the panel (enemy / other types included). Actions are simply not shown in the modal code when the selected entity is not owned (not a universal rule).
- Hint element removed (tutorial / tooltips later). Done inside task A.
- Calendar at the top of the screen: current day + time left until the next daily tick. Needs a client-readable global world state (age, TICKS_PER_DAY); share the mechanism with the treasury readout (task C).
- Global game parameters: one constexpr static structure holding every global constant of the world sim (ticks per day, rates, thresholds, magic numbers), exposed to the client through the backend, with a GDO for easy reading. World state values (age, treasury) are exposed property by property.
- Legacy debug text panel stays; add a way to collapse / hide it (click and/or hotkey).

- Genericity pass first: the entity inspection panel must show any entity type, not just armies / settlements with troop breakdowns. Complex in-panel elements reusable across panels; panels (except special ones like the action panel) generic and able to hold anything. No two different panel elements per inspected entity type (settlements and armies share elements). Multi-file client change.
- Treasury gold count always visible, bottom left (position may change).
- Settlement inspection panel: local wealth with last day's change beside it; hover shows a floating breakdown window.
- Population count with growth / shrink next to it.
