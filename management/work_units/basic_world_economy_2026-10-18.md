# Basic world economy

Started: 2026-10-18 (renamed from `alpha_wealth_treasury`; absorbs `alpha_settlement_buildings` and `alpha_trade`). Design: `management/design_doc.md`, Alpha > Settlements > Local Wealth & Construction, Upkeep & Economy, Buildings, Trade.

## Scope

- Simulation time: calendar days (N ticks), daily ticks as the unit economy mechanics hook into.
- Local wealth per settlement (sources: land output, buildings, caravans), base cost per population, taxation into the owner's single treasury, treasury spending hooks. HUD display and tax control.
- Settlement buildings: definitions (effect, ticks, cost, max level) and in-settlement construction on top of the worker / work type system from `road_construction_2026-10-03.md`. Timed and financed from local wealth (no wealth = suspended). Housing (pop cap) first. Roads are instant and free today, buildings are timed and paid.
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
3. Buildings (on workers; see Buildings design + decisions):
   - a. [DONE] Settlement building data: building type table (rows with optional events) in `world_entities.h` next to per-settlement counts; `troop_max` / `troop_training` in `settlement_population` + GDO; per-mille params and `troop_growth_per_day` removed; daily building tick as own system; Barracks handlers, training and max hand-tested OK. Housing row dropped on purpose; per-building-type GDO lines wait on dynamic GDO properties.
   - b. Settlement construction worker (in-settlement, `BUILD_SETTLEMENT_BUILDING`, data = building type): command, daily financing, stall, job accounting. [DONE, untested] `BUILD_SETTLEMENT_BUILDING` command, `workers` in `settlement_population` (counts in consumption / growth, not land output), `settlement_wealth_fund_from_treasury`. Worker system now runs after the settlement buildings daily tick (was first). No UI; test via console `send_command`.
   - c. Found-settlement worker (on-map, `FOUND_SETTLEMENT`, data = target location + colonist count): moves to the spot, founds a village, else returns home and rejoins the population. [DONE, untested] Command `FOUND_SETTLEMENT {settlement, target}` replaces the army-based one; colonists (150, game parameter) leave population on spawn; workers have `positions` / `movements`, drawn as squares; attack-on-worker not built (hook: `world_resolve_attack`). No UI; test via console.
   - d. [WIP, peer] Client UI: bigger inspection panel over the old action bar, action bar becomes bottom-right action panel (RTS style), "Construction" action switches the inspection panel to the settlement's constructions (levels / counts + ongoing projects). Not started.
4. Trade: not broken down yet.

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

- Buildings (see Buildings design): knights cap before Keep, free starting Barracks, project queue, recruitment rate, split into own unit file.
- Upkeep split for armies (treasury): see `alpha_troops_recruitment`.
- Caravan return trip, or disappear at target?
- What does a caravan carry (plunder gives "carried gold")?
- How are sea lanes established?

## Buildings design (proposed, awaiting user answers)

Direction (user, 2026-10-21): settlement construction runs on worker entities (formerly builders; generalized to bounded-life civilian processes, design grows from real uses, no own work unit), plus a settlement-specific upgrades / buildings mechanic. First application: local troops max and training rates. The "project slot / daily tick pays cost" mechanics below are superseded where workers cover them.

- First building: Barracks (3 of the doc: 5 days, 500 wealth, +10 men-at-arms and +5 horsemen capacity per level). Goal is a general building system; mechanics iterate later. Housing deferred (pop cap meaning unclear in the consumption model).
- Definitions: constexpr table in `game_parameters` (build days, cost, max level, effect params).
- Settlement state: level per building type + one construction project slot (type, days done); GDO.
- Financing: daily tick pays cost / days from local wealth and advances one day; short on wealth = suspended.
- Command `BUILD_BUILDING {settlement, building_type}`: owner only, no running project, below max level.
- Troop caps: `settlement_troop_max` = levels x capacity per type; per-mille parameters removed; levy max stays peasants - 10.
- UI: Buildings section in the inspector (levels, project progress row) + Build actions in the settlement action bar.
- Answers (user, 2026-10-21): knights cap 0 until a Keep exists (yes); no free starting Barracks (maybe some levies raisable immediately / uncapped, irrelevant for now); no project queue: any number of constructions in parallel as long as they do not conflict (two upgrades of the same building cannot run in parallel; new instances of a building can). Every active construction costs per day; short on manpower / resources = stalls. Barracks sets troop max and training rates; values are placeholders. No separate unit file.
- Supersedes above: "one construction project slot" and "level per building type".
- Building storage: every building type is a per-settlement "settlement building" entry with level / count starting at 0. Two kinds: unique (upgraded to a max level: keep, walls, trade center) and multiplied (count grows: housing, barracks, storage). Alpha scope now: multiplied only (barracks, housing). No on-map buildings for now (outside roads).
- Construction manpower: a financed project is a "job" that takes workers out of the peasant pool for as long as it is financed. They still count as settlement population (consumption, growth) exactly like peasants. Financing: money moves treasury -> local wealth as the workers are paid. Open: fixed wealth / day per project vs a player slider.
- Financing: fixed wealth / day per project (from the definition), treasury -> local wealth as workers are paid; cost multipliers later. Unfinanced = stalls, workers return to peasants. Project workers leave base land output but count in consumption / growth. Population categories for Alpha: peasants, workers, soldiers (general job system later maybe).
- No generic start-project command: two concrete operations, found settlement (on-map worker) and settlement construction (in-settlement worker). Shared only: worker entity + work type, separate logic in the worker update system (`world_system_worker_construction`).
- Parallel rule: several Building units of a type can be built in parallel; Upgrades of one unique building cannot.
- Colonists of a founding worker leave the population entirely while out (maybe generalize "raised" to "on the field" later). Worker destroyed when attacked.
- Working rule (user): no game-design concerns until said so; temporary broken mechanics are fine; major systems first.

## Current state

- Wealth, treasury, tax, consumption-based growth, per-player state, calendar and player panel exist (see Next step). Cost hooks `raise_army_can_afford` / `raise_army_charge` are still stubs (match_commands.cpp). Validity error flags mirrored by hand in input.ts.

## Depends on

`straight_line_movement_2026-10-15.md` (trade).

## Next step

State: time system, accounting, daily tick, game parameters, inspection feature, tax command, player state panel (incl. `settlement_count`, `total_population`, `total_troops`), ceiling change, entity helpers + `world_update_player_stats`: all coded. First batch hand-tested OK; untested since: ceiling change (quirk: slider range 0..max, marker can pass the end if home + raised > max), GDO buffer fix (384 -> 1024; command GDO buffer is 128 bytes, largest command 82), `world_player_state` income / deficit, entity helpers (`world_sim_army.h` / `world_sim_settlement.h`, `TROOP_TYPE_*`; player stats lag one tick; disband returns exactly the released amount).

Decided: army gauges show current vs raised count as max; `GAME_PARAMETERS` stays where it is; ceiling targets home + raised (training room = min(ceiling, max) - (home + raised); lowering discards only home troops down to ceiling - raised; `ceiling_*` GDO = effective total ceiling); enemy info visible in inspection panels; remaining constants sweep (territory, battle, movement, army reach, terrain) happens with the systems they belong to; all mutations of population / troops / owner go through the entity helpers.

Housekeeping for the user: delete stale compiled .js in web/src (ui/ui_element_treasury.js, troop_gauges.js, ui_element_hint.js, ui_element_properties_panel.js, mode_hint_ui.js, settlement_bar_ui.js). Imprints that could mention `game_parameters.h`, `world_player_state`, `ui/inspection/` (user's call).

Side task (client-only, user bonus 2026-10-21): road rendering as drawn shapes from the 8-neighbour road mask, same style / color as the path preview; coded in render.ts (`draw_roads`), untested. Roads now controlled only by `show_infrastructure` (not `show_territory`). Road flag change not planned. Settlements now set `is_road` on their tile at spawn (world_sim_settlement.cpp), untested; destroy leaves the road; client road origin check in `road_building.ts` now uses server validity feedback (`get_command_feedback("BUILD_ROAD", ...)`, `ORIGIN_INVALID`, right-hand neighbour as stand-in destination); coded, untested.

Next: user hand-tests the untested items; answers the Buildings design questions; then a builder does the first building task (table, settlement state + GDO, new troop-cap rule). After: `straight_line_movement`, then trade. Smaller candidates: war economy costs / upkeep (`alpha_troops_recruitment`), replacement for found settlement.

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
