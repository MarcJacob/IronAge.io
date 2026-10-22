# Economy revamp

Started: 2026-10-22 (split from `basic_world_economy_2026-10-18.md` after trade play-test). Likely needs a redesign: the model got confusing from trying to keep it simple.

## Problem

- Local wealth equilibrates at 0 (growth scales with excess, so population eats the whole output). Nothing accumulates, so caravans (funded from local wealth) never spawn.
- "Local wealth" is the wrong name for a value that sits at 0 in equilibrium; it is really a surplus. UI should make output / income the thing that matters.

## Decisions

- Trade pool: per-settlement `trade_pool`, funded by a share of the day's effective output before tax / consumption; caravans are funded from it (replaces funding from local wealth). Housing-based population cap deferred (own feature later).
- Placeholders: `trade_pool_output_percent` 10; `min_trade_pool_to_spawn` 100; `investment_percent` 10 of the pool; caravan cancel refunds the pool; arrival: origin half to origin trade pool, destination half to destination local wealth; homeless: all to destination local wealth.
- Later ideas (not decided): rename local wealth to "surplus" in UI; trade base on output / income instead of accumulated stock; population cap from housing.

## Design direction (user, 2026-10-22; replaces the "local wealth" model, not yet coded)

- Wealth = general unit of "stuff", predominantly keeps people alive. Aim of the economy: grow production and / or income to feed more population / soldiers / taxes. Model should create contention for land and trade routes.
- Four categories, shown clearly in the UI:
  1. Production: generated locally by peasants working land + building-based sources.
  2. Income: production + external sources (trade, wages paid by the treasury to workers). Soldiers and other unproductive population add nothing to income (pure sinks).
  3. Consumption: local population spends what is left of income to live and grow; shrinks if short.
  4. Trade pool: finances caravans and raises the profit of incoming caravans.
- No accumulating local wealth: income not taxed or put into the trade pool is consumed and feeds growth.
- Treasury: tax takes wealth out of the economy for the player (reinvest via reverse tax / buildings with wages going back in, or unproductive spending such as military).
- Trade is semi-controlled: no control over caravan origin / destination; player makes the settlement attractive, uses routes, produces / exports.

- Soldiers consume locally (count as population at home, squeeze the budget). Later experiment: treasury pays part / all of their consumption.
- Caravan revenue goes entirely to income (50/50 origin / destination, arrival day). Nothing goes to a trade pool from revenue. The pool is only refilled by the daily set-aside from income; its purpose is to be turned into income via trade, not to grow as capital.
- Trade profit's "destination wealth" input is the destination's income (last day's `economy.income`); first it was the trade pool, changed 2026-10-22.
- Caravan value at spawn = max(25% of pool, min(100, pool)) (`investment_percent` 25, `investment_min` 100); `min_trade_pool_to_spawn` placeholder 1.
- Trade pool split (2026-10-22): export pool (set-aside from income, finances outgoing caravans, refunds) and import pool (receives caravan revenue, both halves; releases `trade_import_release_percent` (10) of its contents into income each daily tick; revenue that does not fit goes to income immediately). Sizes are separate per building so buildings can specialise; Caravanserai: +100 export, +200 import, literals in its building event functions; description "Increases trade export and import pool sizes." Without trade buildings import cap is 0, so revenue is immediate income.
- Tax applies to total income (production + trade + wages).

## Model (draft, per settlement per daily tick)

- production = raw output (peasants on land) converted to wealth (+ building sources later).
- income = production + pending external income (trade revenue arrived and wages received since the last daily tick; accumulated by the trade / worker systems, consumed and zeroed by the daily tick; fixes the off-tick road payment gap).
- tax = tax_rate % of income, to the owner's treasury.
- trade set-aside = trade_pool_income_percent of income, up to the pool's room (rest stays in the budget).
- consumption budget = income - tax - set-aside. Nothing is stored: unused budget is not kept.
- required = 1 per person at home (peasants + home soldiers + workers).
- r = budget / required; growth curve as today (r >= 1 grows, excess adds up to +2% but is not stored; shortfall shrinks).
- Caravans: investment taken from the pool at spawn; revenue (value x multiplier + profit) split 50/50 into origin / destination pending income; unused-capital refunds (cancel / return home) go back to the pool.
- UI: four elements (Production, Income, Consumption, Trade pool) with per-day figures and tooltip breakdowns; income tooltip lists production / trade / wages and the outflows (tax, set-aside); consumption shows required vs budget and the growth rate; trade pool shows current / max and last set-aside. Treasury readout stays player-level. "Local wealth" disappears as a stock.

## Tasks

1. [DONE, unbuilt] Trade pool: `settlement_trading::trade_pool`, `economy_trade_set_aside`, params `trade_pool_output_percent` 10 / `min_trade_pool_to_spawn` 100; set-aside taken from effective output (capped at income) before tax; spawn / cancel / arrival use the pool; "Trade pool" row (own settlements) and Wealth tooltip line "Set aside for trade". Also fixed: settlement snapshot lacked `.trade`, so `trading_*` GDO read zeros before.

2. [DONE, unbuilt] Pool cap: `trade_pool_max` = `trade_pool_capacity_per_building` (200) x Caravanserai (trade post) count, maintained in the building added / removed events next to `caravan_count_max`; overflow (set-aside, refund, arrival origin half) goes to local wealth; cap below pool removes nothing; UI row shows current / max; GDO `trading_pool_max`.

3. [DONE, unbuilt] Pool fed from income (`trade_pool_income_percent`, not output); "effective output" renamed "raw output" (work units before wealth conversion; income = final); capacity 200 per Caravanserai becomes a literal in the building event functions, not a parameter (may become a building parameter later).

4. [DONE, unbuilt] Model rewrite in C++: `settlement_pending_income {wages, trade}` per settlement (paid by workers / caravan arrivals, consumed by the next daily tick); daily tick production -> income -> tax -> set-aside (capped by pool room) -> budget -> growth from budget / required; `local_wealth` removed (start settlement no longer gets 200); trade profit input = destination `trade_pool`; refund overflow to pending trade. GDO: `economy_production|wages|trade_income|income|tax|trade_set_aside|consumption_budget|consumption_required|raw_output`, `trading_pool`, `trading_pool_max`; struct `settlement_economy` (array `economies`); growth data lives in `settlement_population` (`population_change`, `population_growth_rate`); "finance" renamed "economy" in code. Notes: `world_sim/.ai_imprint/settlement_economy_model.md`.
5. [DONE, unbuilt] UI (TS): Income row + breakdown tooltip, tax slider, Consumption row, Trade pool read-only gauge; old Wealth rows removed. Consumption delta uses `population_change` (people per day); no growth property in the economy GDO.

6. [DONE, unbuilt] Export / import pools: `export_pool|_max`, `import_pool|_max` in `settlement_trading`; GDO `trading_export_pool|_max`, `trading_import_pool|_max`, `economy_trade_release`, `economy_trade_overflow` (overflow also counts export refunds that did not fit), `economy_trade_income` = release + overflow; params `import_release_percent` 10, `min_export_pool_to_spawn` 1; Caravanserai +100 export / +200 import (literals in building events), description extended. Daily order: pending -> overflow, release from import pool, income = production + wages + trade, tax, set-aside to export pool, budget. UI: two gauges (export yellow, import teal), income tooltip splits trade lines.

## Open questions

- Wider redesign of the wealth / consumption / growth model: revisit as more game mechanics arrive.

## Next step

None. Done for now (play-tested OK); revisit later.
