# Trade / caravans

Started: 2026-10-22 (split from `basic_world_economy_2026-10-18.md`, Trade). Design: `management/design_doc.md`, Alpha > Trade. Depends on `straight_line_movement_2026-10-15.md`.

## Scope

Settlements invest local wealth into caravans; caravans path to a destination settlement over existing land pathing and complete one transaction on arrival. Land only, no sea / ships (ship flag on the entity stays reserved).

## Requirements

- Spawn: once per daily tick, per settlement, one independent roll per missing caravan slot. Probability 25% at 1 missing slot, linear up to 75% at 5+ missing.
- No spawn if: caravan max reached, local wealth < min (game parameter, initially 100), no valid destination, zero investment.
- Investment = 10% of origin local wealth, deducted at spawn (only after a destination was found).
- Destination: any other settlement with a valid path from the source. Deterministic weighted random (`world.rand_gen`), weight = potential profit / path cost (handle zero cost / zero profit).
- No intermediate stops. Caravan disappears on arrival.
- Retarget if destination becomes invalid: source = origin if valid, else caravan's current location; same selection logic. No candidate: stay active and retry later, never lose the investment. Counters unchanged on retarget.
- Origin destroyed after spawn: caravan continues, homeless; destination gets the full profit.
- Arrival: profit split 50/50 origin / destination (origin valid), else all to destination; destroy caravan; decrement origin counter exactly once; settlement wealth arithmetic must not wrap.
- Terms: profit = extra value on top of the base; revenue = value x base multiplier (2) + profit, the full payout on arrival (split 50/50). Profit includes a flat additive 10% of value (at least 1), so a caravan never adds nothing and small caravans favour close destinations. Selection weight = profit / cost (not revenue).
- Profit: standalone pure function (caravan value, destination local wealth, travel distance, trade params), also called for candidate weights and for future client prediction; `trade_revenue` wraps it. Bonus from value vs destination wealth (smaller caravan relative to destination wealth = stronger), plus logarithmic distance bonus (fast rise to ~1 region, slow after, plateau ~3 regions, plateau configurable, land only). Distance = total path cost, not time. Wide integer intermediates.
- Trade params in a dedicated substructure of `game_parameters` (`include/game_common/match/game_parameters.h`) with GDO properties.
- Trade processing is its own system after movement in `world_state_tick`; spawn runs with the daily systems.
- Caravan references (origin, destination) must be validated at every use: settlement destroy only invalidates the slot.

## Repo findings

- `settlement_trading` (active / max caravans) exists in settlement entities; caravan entity exists (location, movement, origin, destination, value, ship flags). `world_spawn_entity_caravan` / `world_destroy_entity_caravan` exist but do not maintain counters; caravan container init (max_count, active_count, guids, zeroed arrays) is incomplete.
- `world_request_path` returns `path_result` with `length` in tiles only (no cost); existence = `count > 0 && !partial`; `start == end` gives count 0. A* computes cost internally (`g`, chamfer-weighted 5 / 7) but never outputs it. Task 3 blocked on this.
- Daily economy resets `settlement_finances`: trade results need a dedicated field (last profit / income / investment / distance), not the land-economy figures.

## Decisions

- Formula constants are placeholders, tunable. Plan's draft formula has a cancelled term (value x wealth / value): encode the wealth comparison as a bounded ratio instead.
- Caravan value fixed for the journey (ui16 stored); profit uses origin-to-destination path cost, not the travelled route.
- Trade wealth added to local wealth immediately on arrival.
- Selector weight = profit x 1000 / max(cost, 1) (fixed-point, avoids rounding to 0).
- Path `cost` added to `path` / `path_result`; selection weight = profit / cost (distance bonus in profit, cost penalised).
- Terrain from caravan ship flag: land if not ship, water if ship (ships later: coastal building spawning flagged caravans + alternative selection + water pathing). No roads-only mode for now.
- Candidates on the caravan's own tile are skipped (caravans move).
- Task 6 fallback: caravan with no reachable destination returns home, despawns there, value goes back to local wealth with no profit (replaces "wait and retry").
- Pathing cost / optimisation out of scope (`path_store_cache`).
- Working rule: placeholder / temporary decisions are fine; no game-design polish yet.

## Tasks

1. [DONE, unbuilt] Caravan storage init + `game_parameters.trade` + GDO (`trade_<field>`); spawn / destroy maintain origin `caravan_count` (ui8). Also fixed: settlement `trade` array never allocated / not reset on spawn, `world_entity_is_valid` missing `break` for CARAVAN.
2. [DONE, unbuilt] `trade_profit` in `include/game_common/match/world/world_trade.h` (+ `ia_log2_q8` in `include/core/math/numerical.h`). Value bonus by per-mille ratio wealth / (wealth + value); distance bonus by log curve (`distance_curve_mid`, `distance_saturation`, steepness); distances in path-cost units, region size not yet mapped. Reworked, unbuilt: `trade_profit` = value bonus + distance bonus + flat term `max(value x profit_flat_percent (10) / 100, 1)` (always added); `trade_revenue` = base x value + profit; selector weight uses profit.
3. [DONE, unbuilt] `world_caravan_select_destination(world, source, origin_exclude, value, caravan_flags, out_destination, out_path_cost)` in `caravans/world_sim_caravan_destination.cpp`; `cost` added to `path` / `path_result` (goal `g`, 0 if partial / start == end). Weights per tile-cost kept on a 4096-entry stack array; RNG advanced only when a candidate exists. Cost is of the path found (weighted A*), maybe not the cheapest.
4. [DONE, unbuilt] `world_system_caravan_spawn` (`caravans/world_system_caravan_spawn.cpp`): chance roll before destination selection; value capped at ui16 max. Needs `caravan_count_max` > 0 (Caravanserai) and wealth >= 100 to spawn.
5. [DONE, unbuilt] Movement via `world_entity_follow_path` in `world_system_movement.cpp` (land only, speed 4); failed path cancels (`world_caravan_cancel`: refund origin, destroy). Path cost not stored on the caravan: arrival recomputes it from origin tile (homeless / failed = distance 0).
6. Destination invalidation + retargeting pass (currently simple fallback: cancel, refund). [todo]
7. [DONE, unbuilt] `world_system_caravan_trade` after movement: 50/50 split (odd unit to destination), homeless = all to destination, clamped wealth; 8. [DONE, unbuilt, server side] Wired in `world_state_tick` (spawn after buildings daily tick; trade after movement). Client exposure of trade state still open.
9. [DONE, unbuilt] Client: caravans drawn (`art/entity_caravan.svg`), selectable, "Caravan" inspection (Value, Origin, Destination as `Settlement (x, y)`), gold path preview; caravan GDO `value`, `world_entity_query_caravan` defined (was declared only), client query case. Notes in `web_game_client/.ai_imprint/adding_ui_things_howto.md`.
10. [DONE, unbuilt] Distance rework: `trade_profit` distance = `trade_tile_distance` (max + min x 2/5, tiles) origin-destination, same in selector (from source) and arrival; path cost only divides the selection weight; origin gone = distance 0; no path recompute at arrival. `distance_curve_mid` 128 (one region), `distance_saturation` 384 (three regions), steepness 7. `last_trade_*` fields removed.

Tasks 1-5, 7-10 coded; user builds and plays the full loop next, then task 6 and tuning.

## Open questions


- Which trade-result fields clients need (task 7 / 8).
- Caravan return trip: none for now (disappears at target).

## Next step

User builds and reviews tasks 1-3 (check client reads `trade_investment_percent` = 10; `game_client_query.cpp` still compiles with `path::cost`). Then delegate task 4 (daily spawn).
