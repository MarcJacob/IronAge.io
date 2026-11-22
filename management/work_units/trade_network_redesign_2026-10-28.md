# Trade network redesign

Started: 2026-10-28. Design doc: `management/design/economy_design.md`, Trade > Trade Flow. Code map and formulas: `src/game_common/world_sim/.ai_imprint/trade_survey.md`.

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
- Spawn: per settlement per day, max 3 picks, one spawn. Score = profit x 100 / tile distance, eligible if >= `min_profit_per_travel_dist_percent`; weighted random on score (linear). No home-stock factor, no reserve (starvation for a good deal is intended). Caravan size = min(50% of export pool, destination room, 0xFFFF), floor 10. Destination room = capacity - (local + export). New settlements start with 1 caravan slot (`start_caravan_slots`, experimental); Caravanserai +1. Evaluation uses tile distance.
- `allow_foreign_export` per settlement, default FALSE (owner-set via `SET_ALLOW_FOREIGN_EXPORT`): when FALSE only same-owner settlements are trade targets (caravans and ships).
- Colonists take their share of the LOCAL pool only (colonists / total population).
- Inter-player from the start.
- Base pool capacity pop x 4, sellable 25% (stockpile target for sales / growth).
- UI rule: entity information is never hidden by ownership; only controls are owner-only (foreign sliders show disabled).

## Status

Done and hand-tested: survey, trade volume, spawn, export pool, `export_ratio`, foreign export flag, UI.

## Remaining

- Use real path cost instead of tile distance in spawn scoring and profit. Blocked on `pathing_rewrite` (2b / 2c).

## Next step

Wait for the pathing rewrite, then switch trade to path cost.
