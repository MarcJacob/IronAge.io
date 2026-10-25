# Trade / economy rework: Market Goods pool

Started: 2026-10-24. Supersedes the income / export-import pool model of `economy_revamp_2026-10-22.md` and the funding / spawn / profit inputs of `trade_caravans_2026-10-22.md`. Phase 4 supersedes the refill / homecoming decisions of `army_supplies_2026-10-23.md`. Local troop pool of `alpha_troops_recruitment_2026-10-18.md` is slated for removal (phase 2).

## Goal

- Understandable, self-balancing economy: Market Goods pools and populations drift to equilibrium.
- Emergent army supply lines through caravans.

## Model (user, 2026-10-24)

- Single Market Goods pool per settlement replaces export / import pools. No income concept: only flows of goods to / from the pool.
- Chain: Production -> taxed -> + pool -> Sales / Consumption -> pop growth.
- Only production is taxed for now (taxing trade profit: to consider).
- All post-tax production goes to the pool.
- Sales = portion of the pool turned into income-like value, depending on consumption demand (population) and pool fill %. Pool should never quite hit 0.
- Scarce goods (sales cannot cover consumption) = growth penalty / shrinkage. Capping population is a tool to let goods accumulate for export.
- Pool level drives caravans: low = higher profit for incoming caravans, lower spawn chance, smaller caravans; high = inverse.
- Pool size: base = population, plus buildings (like now).
- Wages removed as a concept: paying workers / soldiers is just spending so they can consume.
- Housing buildings + max population: later, own step.
- Separate trade tax: to consider.

## Phase 1 decisions

- Sales, two layers. Layer 1: `sellable_percent` of the pool is the most that can be consumed per daily tick (100% = pool can be emptied). Layer 2: strata soldiers / workers / peasants, each with a share percent of the sellable amount (soldiers X%, workers Y%, peasants remainder). Each stratum takes what it needs up to its share; unused share cascades to the next stratum (soldiers -> workers -> peasants). Peasants also get the remainder of the sellable pool. Purpose: soldiers starve before the last peasant / worker does, not after.
- Growth: same formula as now, driven by consumption satisfaction (met = grow, nearly met = stagnate, low = starve), per stratum.
- Capacity: base = population, plus building contributions (Caravanserai / Trade Depot literals stay as placeholders).
- Tax on production only. All post-tax production goes into the pool. No income concept, no wages.
- Soldier and worker pay come from the treasury (not from taxes) and vanish; pays what the treasury has, no consequences if short.
- Worker pay: each worker entity holds a daily cost field; one common system takes the total from the treasury (origin-dependent sourcing / destination of the money: later). Replaces per-caller `settlement_pay_from_treasury` calls in construction.
- No overflow: everything goes into the pool, which may exceed capacity. Capacity is only the reference for fill % (curves clamp at 100%). Later: faster emptying of over-capacity pools (incl. a share of pure waste).
- Caravans funded from the pool; spawn chance / size scale with pool fill %, incoming-caravan profit inversely (placeholder curves); arrival split 50/50 into the pools.

## Planned phases (order decided 2026-10-24)

1. Economy only: Market Goods pool, Sales formula, production into pool, spawn / profit driven by pool level, wages dropped. Local troops stay normal local consumers; their pay is taken from taxes and vanishes (only lets them consume, no consequence if unpaid). Done when peasants, workers and settlement-to-settlement trade work.
2. Armies adapt: armies become trade targets (generalized trade-target entities, multi-file architecture change; profit formula does not reward distance); army supply via caravans (maybe exception inside a settlement).
3. Local troops rework, undecided: settlements keep supporting them as consumers until then. Candidates: troops always part of an army with own supplies (stationed armies resupply from the pool; settlement consumption = peasants + workers), or settlements training soldiers locally.

## Flags

- Phase 3 would touch troop pool, ceilings, raising from pool, siege defenders, entity panel sliders (`alpha_troops_recruitment`), road workers (`road_system`), army supplies refill. Large, multi-file.
- Uncommitted edits in `game_parameters.h` / `world_system_settlement_economy.cpp` (old economy tuning): user to commit or drop before phase 1.

## Phase 1 tasks

1a. [DONE, unbuilt] Pay from treasury and vanishes (`settlement_pay_from_treasury`); `worker_pay` / `soldier_pay` removed from economy + pending income structs and GDO. `section_economy.ts` still reads `economy_worker_pay|soldier_pay`. `game_mechanics.imprint.md` pay lines out of date (user edits).
1b. [DONE, unbuilt] Market Goods pool, sales cascade, per-stratum growth (births all peasants; starving strata lose their own people), capacity (people at home + building sums, Caravanserai +300, Trade Depot +1400), caravan spawn / size / profit by pool fill, workers `daily_cost` + `financed` with common pay system `world_system_workers_pay`. New params in `trade`: `sellable_percent` 50, `soldier_share_percent` 20, `worker_share_percent` 20, `min_pool_to_spawn` 100, `investment_percent_min|max` 10 / 40. Notes: `world_sim/.ai_imprint/settlement_economy_model.md`.
2. [DONE, unbuilt] Client UI (`ui/inspection/section_economy.ts`, `section_trade.ts`, `inspect_entity.ts`): Production row + tooltip, one "Goods sold" (sold / needed) row with starvation-order tooltip (strata not named in UI), Market Goods gauge. Was: `section_economy.ts` / `section_trade.ts` read removed GDO props (`economy_income`, `economy_trade_*`, `economy_consumption_*`, `trading_export|import_pool*`, worker/soldier pay); new: `economy_sellable`, `economy_{soldiers,workers,peasants}_{required,share,consumed,growth_rate}`, `trading_market_goods`, `market_capacity`.

## Decisions (builder pass)

- Peasant excess (offered above need) drives bonus growth, not removed from the pool. Soldier need 1 per person. Starving soldiers / workers die like peasants.
- Later ideas: further ways to empty over-capacity pools; workers prefer staying peasants when goods are scarce; worker money source by origin entity.

- Start goods: new settlements get pool = population x `start_goods_percent` (100); a settlement cannot start at exactly 0 (0 = use default).
- Workers: no separate finance system; `worker_pay_day` pays all-or-nothing only when a project actually advances (building: crew first, then pay; road crew pays first tick + daily, `funded` flag).
- Barter profit model (supersedes the demand-premium formula; unbuilt): `trade_profit(value, dest_pool, dest_production, dest_capacity, distance, params)`. Destination values the caravan's goods at r (200% at empty pool down to 50% at full, linear in fill; origin scarcity unused). available = pool + last daily production (`economies[i].production`); E = min(v, available / r). P = E x margin(distance) + flat (flat = `investment_min` x 10% = 10, capped at E). Margin: 0 at `distance_margin_min_tiles` (8), log curve to `distance_margin_max_percent` (50) at `distance_saturation` (256). Scarcity limits volume only, never scales P. Split 50/50. Selector: weighted random (`world.rand_gen`), weight = (P x 1000 / tile distance)^2 (ratio clamped at 2^20, ui64; exponent constant `CARAVAN_WEIGHT_EXPONENT`), linear cost placeholder; retarget excludes the just-failed destination for that selection. `sellable_percent` is for local consumers only (rename possible). Later ideas: spawn caravans after choosing a destination; caravan profitability modifier from home scarcity at departure.
- Over-capacity waste: after consumption, `overflow_waste_percent` (25) of (pool - capacity) disappears each daily tick. Waste display waits for the UI architecture refactor (`web_ui_static_modules`), which will also bring the complete economy UI.
- Spiral fix: no base multiplier. Arrival: origin gets `v + profit/2`, destination gets only `profit - profit/2` (homeless: destination gets profit, capital lost; goods created per trip = profit). Flat profit = `investment_min` x `flat_profit_percent_of_min` (10%) = 10, additive, floor 1; profit may not be 0. Selector cost linear `tradeDist` (placeholder until real path cost is usable); `caravan_value <= tradeDist` skip removed. Waste stays the main anti-overflow mechanic. No debug logs.
- Fixed: removing a Caravanserai lowers `caravan_count_max`.

## Open questions

- Two land-output bugs found (`world_sim/.ai_imprint/economy_production_investigation.md`): integer division in raw output diminishing returns (sawtooth cliff at peasants = 2 x base land output, causes the pop cycle); `ui8` wrap in tile proximity falloff step count (`world_system_settlement_territory.cpp:29`, causes huge land output). Both fixed (unbuilt): smooth `B + s*B/(B+s)` in ui64; `ui16` step count. User tunes falloff step afterwards. Open: should land output limit territory (currently only population does)?
- `ticks_per_day` is 10 in `game_parameters.h`; `game_mechanics.imprint.md` says 100 (user).
- Profit spiral fix applied (unbuilt, awaiting play-test); investigation: `world_sim/.ai_imprint/caravan_profit_spiral_investigation.md`.
- `game_mechanics.imprint.md` stale: wealth / tax / treasury section, consumption, building pay lines, road crew cost (user edits).

## Next step

Phase 1 committed (587a901, 8ed0e9e): pool, barter profit model, selector, land output fixes, parameters adjusted. Ongoing: tuning only. Then phase 2 (armies as trade targets). Complete economy UI waits for `web_ui_static_modules`.
