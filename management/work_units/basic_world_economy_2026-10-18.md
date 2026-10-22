# Basic world economy

Started: 2026-10-18 (renamed from `alpha_wealth_treasury`; absorbs `alpha_settlement_buildings` and `alpha_trade`). Design: `management/design_doc.md`, Alpha > Settlements > Local Wealth & Construction, Upkeep & Economy, Buildings, Trade.

## Scope

- Simulation time: calendar days (N ticks), daily ticks as the unit economy mechanics hook into.
- Local wealth per settlement (sources: land output, buildings, caravans), base cost per population, taxation into the owner's single treasury, treasury spending hooks. HUD display and tax control.
- Settlement buildings: definitions (effect, ticks, cost, max level) and in-settlement construction on top of the worker / work type system from `road_construction_2026-10-03.md`. Timed and financed from local wealth (no wealth = suspended). Housing (pop cap) first. Roads are instant and free today, buildings are timed and paid.
- Trade (land and sea): split into `trade_caravans_2026-10-22.md` (land first). Sea trade lanes stay in Alpha, later.

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
   - b. [DONE, untested] Daily economy tick: base land output, effective output, wealth, tax, consumption, growth. Per-player `world.player_states[player]` (`world_player_state`: treasury, income, expenses, net_change; GDO below the struct; deposit-only treasury), `tax_rates` signed, `settlement_economy` (superseded by `economy_revamp_2026-10-22.md`; local wealth is gone); GDO: `base_land_output`, `tax_rate`, `economy_*`. Territory `pressure` kept (used by expansion). To check: spawn starvation (500 peasants vs one tile), stockpile behavior.
   - c. [DONE, untested] Set-tax command `SET_SETTLEMENT_TAX_RATE {settlement, tax_rate}`: owner only, within `tax_rate_min` 0 / `tax_rate_max` 10 (game parameters; negative = future investment). Slider next to local wealth in the settlement inspector, negative blue / positive yellow, owned only.
   - d. [DONE, untested] UI: treasury readout, calendar, wealth / population rows (see task B notes).
3. Buildings (on workers; see Buildings design + decisions):
   - a. [DONE] Settlement building data: building type table (rows with optional events) in `world_entities.h` next to per-settlement counts; `troop_max` / `troop_training` in `settlement_population` + GDO; per-mille params and `troop_growth_per_day` removed; daily building tick as own system; Barracks handlers, training and max hand-tested OK. Housing row dropped on purpose; per-building-type GDO lines wait on dynamic GDO properties.
   - b. [DONE] Settlement construction worker (`BUILD_SETTLEMENT_BUILDING`): command, daily financing, stall, job accounting, `workers` in `settlement_population`, `settlement_wealth_fund_from_treasury`. Hand-tested OK.
   - c. [DONE] Found-settlement worker (`FOUND_SETTLEMENT {settlement, target}`): colonists (150, game parameter) leave population on spawn; workers have `positions` / `movements`, drawn as squares; attack-on-worker not built (hook: `world_resolve_attack`). Hand-tested OK.
   - d. [DONE] HUD grid (player state / inspection / action panel), panel areas, optional extra GDO in entity inspection (settlement construction projects), array GDO properties, building definitions (name, description) exposed to the client, Construction section with build buttons, found-settlement mode. Left: building icons, current-effect text in descriptions, project workers not drawn on the map.
4. Trade: moved to `trade_caravans_2026-10-22.md`.

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
- Growth curve (r = wealth available / required consumption): r >= 1 -> +2% (+ up to +2% by excess / consumption, capped at excess == consumption; excess not consumed), 0% at r = 0.9, -20% at r = 0.5 and below; linear between. "Available" = whole local wealth after tax, stockpile included. No food vs wealth distinction for now.
- Consumption: 1 wealth per person at home per day. Small-population percentages get sensible minimums (min +1 when growing, min -1 when shrinking).
- Every transaction to / from local wealth and treasury must be individually determinable (clients need an accurate breakdown). Financial breakdown: per-settlement struct of last day's figures (income, tax, consumption, excess, growth rate) written by the daily tick, exposed via GDO.
- Battle casualties: deducted from the army AND from the home settlement (raised counts and total), freeing room in pools / peasants.
- Single-production / needs-consumption model (food vs commercial wealth) deferred; revisit with buildings / trade.
- Day-length constant lives in the main world_sim header, with a helper telling whether the world is currently on a daily tick.
- Sea trade stays in Alpha: only the definition of links between settlements and the pathing differ from land.
- Caravans travel only on roads (land) / established lanes (sea).

## Open questions

- Buildings: knights cap before Keep, free starting Barracks, recruitment rate.
- Upkeep split for armies (treasury): see `alpha_troops_recruitment`.
- What does a caravan carry (plunder gives "carried gold")?
- How are sea lanes established?

## Buildings design (decisions)

- Settlement construction runs on worker entities (generalized bounded-life civilian processes; design grows from real uses, no own work unit), plus a settlement-specific buildings mechanic. First application: local troops max and training rates.
- First building: Barracks (5 days, 500 wealth, +10 men-at-arms and +5 horsemen capacity per level; placeholders). Housing deferred.
- Definitions: constexpr table; building storage per settlement with level / count starting at 0. Two kinds: unique (upgraded to max level: keep, walls, trade center) and multiplied (count grows: housing, barracks, storage). Alpha scope: multiplied only. No on-map buildings (outside roads). Unique-building Upgrades stub undesigned.
- Troop caps: `settlement_troop_max` = levels x capacity per type; per-mille parameters removed; levy max stays peasants - 10. Knights cap 0 until a Keep exists.
- No project queue: any number of parallel constructions as long as they do not conflict (two upgrades of the same unique building cannot; new instances of a multiplied building can). Unfinanced = stalls, workers return to peasants.
- Construction manpower: a financed project is a job taking workers out of the peasant pool while financed. Workers leave base land output but count in consumption / growth. Population categories: peasants, workers, soldiers.
- Financing: fixed wealth / day per project (from the definition), treasury -> local wealth as workers are paid; cost multipliers later.
- No generic start-project command: two concrete operations, found settlement (on-map worker) and settlement construction (in-settlement worker). Shared: worker entity + work type, separate logic in the worker update system (`world_system_worker_construction`).
- Colonists of a founding worker leave the population entirely while out. Worker destroyed when attacked.
- Working rule (user): no game-design concerns until said so; temporary broken mechanics are fine; major systems first.

## Current state

- Wealth, treasury, tax, consumption-based growth, per-player state, calendar, player panel, buildings and workers exist. Cost hooks `raise_army_can_afford` / `raise_army_charge` are still stubs (match_commands.cpp). Validity error flags mirrored by hand in input.ts.
- Untested quirk: ceiling slider range 0..max, marker can pass the end if home + raised > max. Player stats lag one tick.
- Side task done: roads rendered as drawn shapes (`draw_roads`, render.ts), controlled by `show_infrastructure`; settlements set `is_road` on their tile at spawn; client road origin check uses server validity feedback.

## UI wishes (user, 2026-10-20), remaining

- Legacy debug text panel stays; add a way to collapse / hide it (click and/or hotkey).

## Next step

Trade continues in `trade_caravans_2026-10-22.md`. Then: army upkeep / cost hooks (`alpha_troops_recruitment`). Housekeeping for the user: delete stale compiled .js in web/src (ui/ui_element_treasury.js, troop_gauges.js, ui_element_hint.js, ui_element_properties_panel.js, mode_hint_ui.js, settlement_bar_ui.js). Backlog: `debug_test_commands`, `client_game_data_store`.
