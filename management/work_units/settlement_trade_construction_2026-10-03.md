# Settlement trade and construction

Started: 2026-10-03 (as `settlement_econ_tick`, renamed 2026-10-12)

## Goal

Next settlement economy mechanics on top of the existing population growth / land output:
trade and construction (roads first, other on-map constructions later, using tile flags).

## Done (from the former econ tick scope)

- Population growth toward a target = land output + local wealth; land output from owned
  tiles weighted by influence and contestation (see `village_influence_2026-10-03.md`).
  Good enough for now; capacity = `pop + 10 + pop / 2`.

## Decisions (construction, 2026-10-14)

- Focus: construction, roads first. Trade comes after.
- Roads are tile flags, not entities.
- Instant build for now, paid from the originating settlement's local wealth (no treasury exists). Eventually: worker units doing the work on the map, interceptable.
- Road origin: the settlement or any existing road tile inside its territory.
- Road destination: any point inside the settlement's own territory, or a wilderness point reached by a direct path from the settlement that crosses no other territory (much higher cost per wilderness tile).
- Wilderness = no owner at all. Never crosses enemy or other settlements' territory.
- Two settlements are linked when their roads meet.
- Construction command shape: origin settlement (or origin entity, depending on the construction type) where workforce and means come from, plus a target world location. Each construction type interprets the pair its own way.
- Road: builds from origin to target along the most efficient path, including existing roads. The sim pathfinds.
- Road cost: flat per tile, based on the tile's terrain travel cost, plus an extra for wilderness tiles. Road tiles lower the tile's travel cost to a fixed value.
- Influence was replaced by a simpler "pressure" system (population vs territory size, tile output scaled by proximity). Roads lower proximity: building a road flags the tiles as needing a proximity recalc, which updates everything.
- Other constructions, same tile-flag mechanism: ports, bridges, outposts (raise expansion pressure on a border, harder to push; also buildable inside settlements).

- Local wealth exists per settlement (only grows via caravans today). User will add a land-output income and remove / heavily modify the current caravan trading. "Treasury" is a separate later concept (taxation).
- Road pathfinding: ordinary shortest path over tiles owned by the origin settlement or unowned; sources = settlement tile + its road tiles. Not a literal straight line.
- `is_road` / `proximity_recalc` tile flags and `RENDER_TILE_FLAGS::IS_ROAD` were already declared; new commands go through `management/artisan_skills/add_command.md`.
- Water and coast tiles are never walkable for roads (nor later for land entities). Mountains stay walkable at their terrain cost.
- Wilderness extra cost affects construction cost only, never proximity.
- Pathfinding must be reusable by other features (entity pathfinding, water paths), not road-specific.
- Wealth cost of a road = path cost x placeholder conversion constant; pathfinder budget = wealth / that constant.
- Road cost 1 vs coast cost 1 is moot once water is excluded.

## Tasks

- [DONE, awaiting user review / compile] Prep: shared `world_tile_travel.h` (cost table, neighbour helpers, `TILE_ROAD_TRAVEL_COST` = 1, `TILE_WILDERNESS_EXTRA_COST` = 10, placeholders).
- [DONE, awaiting review] `world_find_road_path` (`world_pathfinding.cpp`): bounded Dijkstra in a search window, caller supplies scratch arena (13 bytes per window tile) and clears it.
- [DONE, awaiting review] Proximity uses road cost (claim + recalc); decrease propagation verified by reasoning, one ring per tick.
- [WIP, peer] Make `world_find_road_path` a generic reusable pathfinder (walkability / cost rules as parameters; land vs water), water tiles excluded for roads, and settle where its scratch memory comes from.
- [ ] BUILD_ROAD command (origin settlement + target location; validity; apply sets `is_road` + `proximity_recalc`, charges wealth; apply re-checks silently, validity is not run before apply). Waits on the pathfinder / memory work.
- [ ] Client: render road flag; input flow (UI to decide).
- [ ] Settlement income from land output (user).

## Open design

- Wilderness extra cost value; the fixed road travel cost value.
- Road proximity effect (how much the proximity is lowered).
- Road movement bonus for armies / caravans (comes with trade / army pathing).
- Tick cadence relative to the sim tick.
- Trade: inputs, caravans, local wealth, trade attractivity; see design_doc.md.

## Depends on

`village_influence_2026-10-03.md`.

## Progress

Groundwork done (see Tasks). Next: user + peer work on the reusable pathfinder and its scratch memory, then BUILD_ROAD command.

## Peer brief

Goal: rework `world_find_road_path` (`src/game_common/world_sim/world_pathfinding.cpp`, declared in `world_sim.h`) into a reusable pathfinder for roads, later entity pathfinding, and water paths. Work with the user on where the scratch memory comes from.

Scope / files: `src/game_common/world_sim/` (`world_pathfinding.cpp`, `world_tile_travel.h`, `world_sim.h`, `world_sim_main.cpp`); wherever memory (arenas) is owned in game common / the match.

Imprints: `src/game_common/game_common.imprint.md`, `include/game_common/include_game_common.imprint.md`, `include/game_common/match/include_match.imprint.md`, others in folders touched.

Decisions already made:
- Water and coast tiles are not walkable for roads; mountains are. Water paths (later) must be possible with the same code.
- Wilderness (unowned tile) extra cost is a construction cost concern, never proximity.
- Current implementation: Dijkstra, sources = settlement tile + owned road tiles at cost 0, 8-neighbour with weights 5 / 7, indexed binary heap, search window = settlement bounds grown by `max_cost / (road cost * 5)`, 13 bytes per window tile, caller supplies and clears a `mem_arena& scratch`. Placeholders: road cost 1, wilderness extra 10.
- Determinism required (server and clients run the same code in lockstep).
- BUILD_ROAD apply is not preceded by a fresh validity check, so apply may run the pathfinder; validity may also.

Done criteria: generic pathfinder API (walkability + cost rules supplied by the caller, land vs water), roads exclude water; a settled answer for which arena provides scratch memory, its sizing and who clears it, usable from command validity / apply and tick systems.

Open questions: which existing arena can hold scratch (per match, per tick, frame-like arena?); window size vs max cost bound; hooking per-entity pathfinding later (path storage).

Note: nothing in this repo has been compile-checked since the groundwork.
