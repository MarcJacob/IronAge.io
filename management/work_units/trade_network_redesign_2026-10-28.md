# Trade network redesign

Started: 2026-10-28. Supersedes the barter-profit model of `trade_market_goods_2026-10-24.md` and `trade_caravans_2026-10-22.md` (caravan movement code reused). Army supply via trade (phase 2 of `trade_market_goods`) builds on this. Design doc: `management/design/economy_design.md`, Trade > Trade Flow. Code map and formulas: `src/game_common/world_sim/.ai_imprint/trade_survey.md` (6b trade volume, 6c arrival / profit, 6d threshold spawn, 6e export pool).

## Goal

- Model the gain from trade networks and their hierarchy: small settlements feed bigger ones, bigger ones send caravans further. Subtle, favours large settlements; small trades stay local.
- Trade centers emerge from player investment (later also military supply lines). Intentionally "unfair": early success snowballs.
- Wealth from trade stays tied to production; no goods from thin air.

## Model (decided)

- Caravans move goods (origin export pool to destination pools, conserved). Trade value is a buff, not goods.
- "Trade volume" (`settlement_trading.trade_volume`): absolute value. Exporter gains the caravan's value + profit; receiver gains the received value (+ profit x `profit_receiver_percent`, 0 now). Profit is volume only.
- Max trade volume = 2 x pool capacity; gains above it are wasted for that settlement only. Decay linear, max / 30 per day.
- Bonus (linear in volume / max): production +50% at full (before tax); profit of incoming caravans +200% at full.
- Profit = value x (scarcity + distance terms), scaled by relative capacity (+25% / -75% clamp), x destination bonus, x short-trade ramp (0 at <= 8 tiles, full at 16). Stateless ping-pong guard (no partner memory). Destination fill uses local + export total.
- Export pool: each settlement has a player-set `export_ratio` (0-90%, default 20). That share of ALL incoming goods (production, imports, refunds) goes to an export pool until it holds ratio x capacity; overflow goes to the local pool; excess above the cap (ratio lowered) moves to local at the daily tick. Export pool is never consumed locally. Pools share one capacity; waste applies to the sum.
- Spawn: per settlement per day, max 3 picks, one spawn. Score = profit x 100 / tile distance, eligible if >= `min_profit_per_travel_dist_percent`; weighted random on score (linear). No home-stock factor, no reserve (starvation for a good deal is intended). Caravan size = min(50% of export pool, destination room, 0xFFFF), floor 10. Destination room = capacity - (local + export). New settlements start with 1 caravan slot (`start_caravan_slots`, experimental); Caravanserai +1. Evaluation uses tile distance; real travel cost after pathing optimization.
- Colonists take their share of the LOCAL pool only (colonists / total population).
- Inter-player from the start. Later: embargo / open routes as diplomacy, armies blocking and despoiling caravans.
- Base pool capacity pop x 4, sellable 25% (stockpile target for sales / growth).
- UI rule (user): entity information is never hidden by ownership; only controls are owner-only (foreign sliders show disabled).

## Tasks

1. [DONE] Survey.
2. [DONE, hand-tested] Trade volume field, decay, cap, production bonus.
3. [DONE, hand-tested] Opportunity-first spawn, goods delivery, volume gains, volume-only profit, ramp.
4. Left: tuning (weight exponent, clamps, ramp, bonus); travel cost instead of tile distance (blocked on `path_commitment_rewrite` / `path_store_cache`).
5. [DONE] Trade volume gauge UI.
6. [DONE] Threshold spawn rework (replaced random + sellable gate).
7. [DONE] Export pool + `export_ratio` + `SET_EXPORT_RATIO` command + UI (slider next to Tax, light-blue right-to-left segment on the goods gauge).
8. [DONE, hand-tested] Export pool cap + gauge marker + daily excess-to-local move (after growth, before waste) + waste on the sum of both pools (proportional).
9. [DONE, hand-tested] Info visibility regardless of ownership in the inspection UI.
- Hand-tested by user: a trade center loop (Housing, Caravanserai, Trade Depot, falling export ratio) works; model "works much better".

## Open questions

- Recommendations raised, none started: travel cost in the score (roads have no trade payoff until then); AI export ratio logic (AI keeps 20%); trade statistics for tuning (overlaps backlog `player_stats_analytics`); counterweights to the trade-center snowball (embargo, caravan blocking: military / diplomacy units).
- Urban population seeding: total population placeholder until the urban stratum exists.
- Garrison ceiling gauges on foreign settlements show values, no disabled slider.

## Idea to experiment with: Trade Routes

- Each trade building creates a Trade Route entity linking a sender and a receiver; it spawns caravans at regular intervals.
- Decouples choosing a partner from spawning caravans; could bring real path cost back before pathing is optimized.
- Flags: new entity type (multi-file architecture change); route lifecycle; caravan sizing still adapts. Held: export pool already fixed the motivating problem.

## Next step

Play-test further (user). Then: travel cost (check size of `path_commitment_rewrite` / `path_store_cache`), trade stats counters, AI ratio rule. Trade must be settled before military supply (`army_supplies`, `alpha_troops_recruitment`).
