// Destination selection for trade caravans.

#include "game_common/match/world/world_trade.h"

// Weight of a candidate: potential profit (the extra on top of the base, flat bonus included, not the revenue) per unit of path cost.
// The profit uses the tile distance between source and candidate, the path cost is only the divisor.
// Fixed point with 3 decimals (profit * 1000 / cost), so far candidates do not round down to 0. Zero profit = 0, zero cost counts as 1.
static ui64 caravan_candidate_weight(ui32 caravan_value, ui32 destination_wealth, ui32 tile_distance, ui32 path_cost)
{
    static constexpr ui64 WEIGHT_SCALE = 1000;
    ui64 profit = trade_profit(caravan_value, destination_wealth, tile_distance, GAME_PARAMETERS.trade);
    if (profit == 0) return 0;
    return profit * WEIGHT_SCALE / ia_max(path_cost, (ui32)1);
}

bool world_caravan_select_destination(match_world_state& world, world_location source, entity_guid origin_exclude, ui32 caravan_value,
    WORLD_ENTITY_CARAVAN_FLAGS caravan_flags, entity_guid& out_destination, ui32& out_path_cost)
{
    const world_entity_settlements& settlements = world.entities->settlements;

    path_params params = {};
    if (((ui8)caravan_flags & (ui8)WORLD_ENTITY_CARAVAN_FLAGS::IS_SHIP) == 0)
        params.passable_terrain = PATH_TERRAIN_LAND;
    else
        params.passable_terrain = PATH_TERRAIN_WATER; // Ships are not implemented: a ship-specific selection function comes later.

    // Path cost of each settlement slot, 0 = not a candidate (a valid candidate always has a cost above 0).
    // Macro entity indices are 12 bits, so this covers every settlement slot.
    ui32 pathCosts[4096] = {};
    ASSERT(settlements.max_count <= 4096);

    // Pass 1: paths. Candidates are the other active settlements with a complete path (start == end gives none).
    ui32 candidateCount = 0;
    ui64 totalWeight = 0;
    for (ui16 index = 0; index < settlements.max_count; index++)
    {
        entity_guid guid = settlements.guids[index];
        if (!guid.is_valid() || guid == origin_exclude) continue;

        // Recalculated for every candidate: pathing to every settlement is costly on large maps. To be replaced by the path cache.
        path_result result;
        world_request_path(world, source, settlements.locations[index], params, result);
        if (result.count == 0 || result.partial) continue;

        pathCosts[index] = result.cost;
        candidateCount++;
        totalWeight += caravan_candidate_weight(caravan_value, settlements.local_wealth[index], trade_tile_distance(source, settlements.locations[index]), result.cost);
    }

    if (candidateCount == 0) return false; // No random number drawn.

    // Pick: a random point in [0, totalWeight), or a random candidate if every weight is 0.
    // 32 random bits are enough for most totals, a second draw extends it to 64 bits (modulo bias is negligible).
    bool uniform = totalWeight == 0;
    ui64 range = uniform ? candidateCount : totalWeight;
    ui64 roll = world.rand_gen.next<ui32>();
    if (range > 0xFFFFFFFFull) roll = (roll << 32) | world.rand_gen.next<ui32>();
    roll %= range;

    // Pass 2: find the candidate holding the rolled point. Weights are recomputed from the stored costs (no pathing).
    for (ui16 index = 0; index < settlements.max_count; index++)
    {
        if (pathCosts[index] == 0) continue;

        ui64 weight = uniform ? 1 : caravan_candidate_weight(caravan_value, settlements.local_wealth[index], trade_tile_distance(source, settlements.locations[index]), pathCosts[index]);
        if (roll < weight)
        {
            out_destination = settlements.guids[index];
            out_path_cost = pathCosts[index];
            return true;
        }
        roll -= weight;
    }

    ASSERT(false); // Unreachable: roll < range.
    return false;
}
