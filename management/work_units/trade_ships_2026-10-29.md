# Trade ships

Started: 2026-10-29. Builds on `trade_network_redesign_2026-10-28.md` (caravan spawn / profit / arrival logic is reused as is).

## Goal

Ships work like sea caravans: same trade logic, water pathing instead of land, spawned from the coast.

## Decisions

- Ship = caravan with a ship flag; travels and picks destinations with water pathfinding.
- Coastal settlement = one of its neighbours (settlement tile) is COAST or SEA terrain. Coastal flag derived (stored or computed: survey decides).
- Spawn tile = first COAST / SEA neighbour found. Arrival tile = same lookup on the destination (only coastal settlements are ship destinations). Known to break in rare cases: ignored for now.
- Trade Port building (coastal settlements only): placeholder, same cost and build time as Caravanserai, adds Trade Ship capacity instead of caravan capacity.
- Destination evaluation stays pure tile distance, same as land caravans (both go to path cost when pathing is optimized).
- Ships are faster and can carry more than caravans. Fixed max carry for caravans (instead of % of export pool) is a later idea, not now.
- Assumed, to confirm: ships have their own slots (Trade Port) and own spawn per day; export pool shared with land caravans.

## Tasks

1. [DONE] Survey, notes in `src/game_common/world_sim/.ai_imprint/trade_ships_survey.md`. Facts: `IS_SHIP` caravan flag already exists (GDO exposes flags); `PATH_TERRAIN_WATER` exists (armies / workers cross water); 8 endpoint sites need a dock-tile helper (on-demand neighbour scan, recommended over stored flag); selector already takes caravan_flags (coastal filter = 1 line); ship counters needed in `settlement_trading`; Trade Port = row 4, coastal check in build validity; client `render_entity` needs a flags byte (stride 19 -> 20, update backend.ts); A* node limit (~3000) may make long sea routes partial = unreachable; sea tile cost 7 vs plains 5, ships need a `ship_travel_speed` param.
2. [DONE] Design (decisions above + survey answers: dock = 8 neighbours; ship pass after land pass, own slots and own spawn per day, export pool shared; `max_ship_export_pool_percent` 80; `ship_travel_speed` = 2x caravan default set at spawn; Trade Port copies Caravanserai, adds ship slots; plain rejection when not coastal; flags byte in `render_entity`).
3. [DONE, hand-tested] Sim (Opus). Dock helper `settlement_dock_tile`; `caravan_endpoint` for all endpoint sites; Trade Port = building row 4 with new `requires_coast` def member; entity cap check at spawn; AI unaffected. Notes: `trade_ships_survey.md` IMPLEMENTED section.
4. [DONE, hand-tested] Client: `render_entity` flags byte (stride 20), ship sprite `web/art/entity_ship.svg`, "Ship" panel title, water path preview. Not done: ship slots row in the settlement panel (GDO `trading_ships_active/max` exist), tooltip reason on the disabled Trade Port button. Notes: `src/platform/web_game_client/.ai_imprint/ships_client_support.md`.

## Open questions

- Ship speed / carry values: tune in play-test (ships hug coast at sea cost 7; roads make land caravans faster on roads).
- Long sea routes may be partial (A* node limit) = unreachable.
- Idea for later: replace `requires_coast` with an implementable per-building `build_conditions` function (user decision: keep the bool for now).

## Next step

Tune speed / carry in play; decide on the ship slot row and the Trade Port button tooltip. Then flag [DONE].
