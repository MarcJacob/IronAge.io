# Road system

Started: 2026-10-21 (not worked on yet beyond the current state below). Follows `road_construction_2026-10-03.md` [DONE]. Related: `basic_world_economy_2026-10-18.md` (trade needs the road network).

## Scope

Make roads matter mechanically and give them direction:
- Terrain movement costs (all terrain, roads included) actually drive movement.
- Directional road flag: a tile builds roads toward each neighbour individually (8 neighbours), instead of a single is-road bit.
- Design pass on road destruction / decay.

## Current state

- Road flag: `is_road` bitfield in `include/game_common/match/world/world_terrain.h`; set by the construction system (`world_tile_set_road`, workers/) and at settlement spawn (settlements are road tiles; destroy leaves the road).
- Travel cost: `ROAD_TRAVEL_COST` (1) in `world_movement.h`; road check comes before the territory check, so roads are passable in foreign territory. Movement speed does not use cost yet.
- Road building validity: `match_commands.cpp` (origin = own settlement or own road tile; destination unchecked for road status). Client origin check uses server validity feedback (`road_building.ts`).
- Rendering: `draw_roads` in client `render.ts`: segments from the 8-neighbour road mask, a diagonal is skipped when either shared orthogonal cell is a road; isolated tile = dot. Native renderer (`game_client_render.cpp`) reads `is_road` directly.

## Tasks

Not broken down yet.

## Decisions

- Directional flag is meant to make the rendering neighbour-mask workaround unnecessary (draw exactly the built links) and to make roads mechanically richer.

## Open questions

- Directional representation: 8 bits per tile (per-neighbour links) vs orthogonal only; memory and wire cost (tile flags view read by JS).
- Does building a link also add the reverse link on the neighbour?
- Terrain cost model: cost per terrain type, road cost per link, how cost converts to speed (shared with `straight_line_movement` task 2).
- Destruction / decay: by war (armies, sieges), by neglect (no traffic / no owner), by owner action; repair cost; interaction with territory ownership.
- Should roads stay passable across foreign territory / enemy settlement tiles?
- Trade network graph built from road links (see economy unit).

## Depends on

`straight_line_movement_2026-10-15.md` (straight-line pathing and movement, travel cost in speed) is a dependency of the remaining road work. Its task 2 (speed uses tile travel cost) covers the terrain-cost overlap; do not duplicate it here.

## Next step

Not started. Do `straight_line_movement` first, then break this down; first decide the directional representation.
