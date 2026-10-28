# Trade network redesign

Started: 2026-10-28. Supersedes the barter-profit model of `trade_market_goods_2026-10-24.md` and `trade_caravans_2026-10-22.md` (caravan movement code reused). Army supply via trade (phase 2 of `trade_market_goods`) builds on this. Design doc: `management/design/economy_design.md`, Trade > Trade Flow. Code map and formulas: `src/game_common/world_sim/.ai_imprint/trade_survey.md` (6b trade volume, 6c spawn / profit / arrival).

## Goal

- Model the gain from trade networks and their hierarchy: small settlements feed bigger ones, bigger ones send caravans further. Subtle, favours large settlements; small trades stay local.
- Trade centers emerge from player investment (later also military supply lines). Intentionally "unfair": early success snowballs.
- Wealth from trade stays tied to production; no goods from thin air.

## Model (decided)

- Caravans move goods (origin pool to destination pool, conserved). Trade value is a buff, not goods.
- "Trade volume" (`settlement_trading.trade_volume`): absolute value. Exporter gains the caravan's value + profit; receiver gains the received value (+ profit x `profit_receiver_percent`, 0 now). Profit is volume only.
- Max trade volume = 2 x pool capacity; gains above it are wasted for that settlement only. Decay linear, max / 30 per day.
- Bonus (linear in volume / max): production +50% at full (before tax); profit of incoming caravans +200% at full.
- Profit = value x (scarcity + distance terms), scaled by relative capacity (+25% / -75% clamp), x destination bonus, x short-trade ramp (0 at <= 8 tiles, full at 16). Stateless ping-pong guard (no partner memory).
- Spawn: opportunity first, then caravan sized to it. Reserve = `reserve_days` (3) of local need; one spawn per settlement per day, max 3 picks; min worthwhile size = % of origin pool (2% all slots free, 15% last slot / single slot); size = min(investable, destination room), destination room = capacity - pool. Weighted random on (exporter gain / tile distance)^2. Evaluation uses tile distance; real travel cost after pathing optimization.
- Inter-player from the start. Later: embargo / open routes as diplomacy, armies blocking and despoiling caravans.
- Base pool capacity pop x 4, sellable 25% (user, after play-test 1 saw no caravans).
- Spawn rework (decided 2026-10-28, replaces weighted random + sellable gate; export slider dropped): caravan spawnable whatever the goods level; best candidate spawns if profit per tile travelled (ratio vs caravan value, percent) >= parameter `min profit per travel dist percent`, after a home scarcity malus (weight falls as origin pool drops). Sellable 25% stays as stockpile target (shapes malus, no longer gates). Caravan size = min(origin pool, origin capacity x `max_caravan_capacity_percent` (10), destination room); no sellable / reserve cap. No hard floor: starvation for a very good deal is intended (like taxation).

## Tasks

1. [DONE] Survey.
2. [DONE, hand-tested] Trade volume field, decay, cap, production bonus.
3. [DONE, hand-tested] Opportunity-first spawn, goods delivery, volume gains, volume-only profit, ramp. User: new trading model works far better.
4. Dissolved into 3. Left: tuning (weight exponent, clamps, ramp, bonus); travel cost instead of tile distance (blocked on `path_commitment_rewrite` / `path_store_cache`).
5. [DONE, unbuilt] UI: Trade volume gauge below Market Goods, both gauges thinner.
6. [DONE, unbuilt] Threshold spawn rework (see Model). Replaces the export slider idea. Scarcity factor linear: 0 at fill = sellable (25%), 1.5 at full (1.0 at 75%); weight = score (linear); params `min_profit_per_travel_dist_percent` (5 in code, user wants ~50; confirmed curve and linear weight), `home_scarcity_max_factor_percent` (150); `reserve_days` removed.

## Open questions

- Destination room = capacity - pool: no room when pools sit at or above capacity. Watch; fixes: higher target, or delivery beyond room.
- Urban population seeding: total population placeholder until the urban stratum exists.

## Idea to experiment with (user, end of 2026-10-28): Trade Routes

- Each trade building creates a Trade Route entity linking a sender and a receiver; it spawns caravans at regular intervals (route itself upgradeable, or one caravan with duplicates).
- Decouples choosing a partner from spawning caravans; routes are not recomputed per spawn, so real path cost can come back before pathing is optimized.
- Flags: new entity type (multi-file architecture change); route lifecycle (destination destroyed, owner change, border change, disconnection); caravan sizing still adapts per spawn; route choice would become a decision made at creation or periodic re-evaluation.

## Next step

Build + hand-test task 5 (TS build, hard reload). Then decide: trade route experiment (discuss design first, then survey of entity creation cost) vs tuning vs military design. Trade must be settled before military supply (`army_supplies`, `alpha_troops_recruitment`).
