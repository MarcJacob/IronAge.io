# Economy revamp

Started: 2026-10-22 (split from `basic_world_economy_2026-10-18.md` after trade play-test). Likely needs a redesign: the model got confusing from trying to keep it simple.

## Problem

- Local wealth equilibrates at 0 (growth scales with excess, so population eats the whole output). Nothing accumulates, so caravans (funded from local wealth) never spawn.
- "Local wealth" is the wrong name for a value that sits at 0 in equilibrium; it is really a surplus. UI should make output / income the thing that matters.

## Decisions

- Trade pool: per-settlement `trade_pool`, funded by a share of the day's effective output before tax / consumption; caravans are funded from it (replaces funding from local wealth). Housing-based population cap deferred (own feature later).
- Placeholders: `trade_pool_output_percent` 10; `min_trade_pool_to_spawn` 100; `investment_percent` 10 of the pool; caravan cancel refunds the pool; arrival: origin half to origin trade pool, destination half to destination local wealth; homeless: all to destination local wealth.
- Later ideas (not decided): rename local wealth to "surplus" in UI; trade base on output / income instead of accumulated stock; population cap from housing.

## Tasks

1. [DONE, unbuilt] Trade pool: `settlement_trading::trade_pool`, `finance_trade_set_aside`, params `trade_pool_output_percent` 10 / `min_trade_pool_to_spawn` 100; set-aside taken from effective output (capped at income) before tax; spawn / cancel / arrival use the pool; "Trade pool" row (own settlements) and Wealth tooltip line "Set aside for trade". Also fixed: settlement snapshot lacked `.trade`, so `trading_*` GDO read zeros before.

2. [DONE, unbuilt] Pool cap: `trade_pool_max` = `trade_pool_capacity_per_building` (200) x Caravanserai (trade post) count, maintained in the building added / removed events next to `caravan_count_max`; overflow (set-aside, refund, arrival origin half) goes to local wealth; cap below pool removes nothing; UI row shows current / max; GDO `trading_pool_max`.

3. [DONE, unbuilt] Pool fed from income (`trade_pool_income_percent`, not output); "effective output" renamed "raw output" (work units before wealth conversion; income = final); capacity 200 per Caravanserai becomes a literal in the building event functions, not a parameter (may become a building parameter later).

## Open questions

- Wider redesign of the wealth / consumption / growth model after the user tries this.

## Next step

User builds and plays (check pool fills ~10% of output / day, caravans spawn at pool >= 100 with a Caravanserai); then decide whether the redesign starts.
