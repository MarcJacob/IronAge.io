// Main implementation file for symbols related to caravan entities.

#include "world_sim_entity_caravan.h"
#include "world_sim_entity_settlement.h"
#include "game_common/match/world/world_state.h"
#include "game_common/match/world/world_trade.h"

// Returns the trade state of a caravan's origin settlement, or nullptr if the origin is invalid (never set, destroyed, or its slot reused).
static settlement_trading* caravan_origin_trade(match_world_state& world, const entity_guid& origin)
{
    if (origin.get_type() != ENTITY_TYPE::SETTLEMENT || !world_entity_is_valid(world, origin)) return nullptr;
    return &world.entities->settlements.trade[origin.get_index()];
}

entity_guid world_spawn_entity_caravan(match_world_state& world, const world_entity_caravan& start_state)
{
    world_entity_caravans& caravans = world.entities->caravans;

    ui16 freeIndex = 0;

    while (freeIndex < caravans.max_count && caravans.guids[freeIndex].is_valid()) freeIndex++;
    ASSERT(freeIndex < caravans.max_count); // Out of memory for caravans.

    entity_guid newCaravanGuid = entity_guid_new(ENTITY_TYPE::CARAVAN, freeIndex, world.age % 1000);
    caravans.guids[freeIndex] = newCaravanGuid;
    caravans.active_count++;

    // Apply start state.
    world_entity_caravans::view newCaravan { caravans, freeIndex };
    newCaravan.copy_from(start_state);

    // The origin's active count goes up with the caravan (and down in destroy, exactly once).
    settlement_trading* originTrade = caravan_origin_trade(world, start_state.origin_settlement);
    if (originTrade != nullptr && originTrade->caravan_count < 0xFF) originTrade->caravan_count++;

    return newCaravanGuid;
}

void world_destroy_entity_caravan(match_world_state& world, const entity_guid& caravan)
{
    ASSERT(world_entity_is_valid(world, caravan) && caravan.get_type() == ENTITY_TYPE::CARAVAN);
    world_entity_caravans& caravans = world.entities->caravans;
    ui16 index = caravan.get_index();

    // Only a valid origin is touched: a destroyed settlement (or one whose slot was reused) keeps its own count.
    settlement_trading* originTrade = caravan_origin_trade(world, caravans.origin_settlements[index]);
    if (originTrade != nullptr && originTrade->caravan_count > 0) originTrade->caravan_count--;

    world_entity_caravan_view destroyedCaravan { caravans, index };
    destroyedCaravan.copy_from(world_entity_caravan {});
    caravans.guids[index] = INVALID_ENTITY_GUID;
    caravans.active_count--;
}

bool world_entity_query_caravan(const match_world_state& world, entity_guid entity, world_entity_caravan& out_state)
{
    if (entity.get_type() != ENTITY_TYPE::CARAVAN || !world_entity_is_valid(world, entity)) return false;

    const world_entity_caravans& caravans = world.entities->caravans;
    ui16 index = entity.get_index();

    out_state.copy_from(caravans, index);
    out_state.owner = world_entity_get_owner(world, out_state.origin_settlement);
    return true;
}

// Allocates the caravans' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_caravans(match_world_state& world, mem_arena& memory, ui32 count)
{
    // Same init as the other containers. memory.alloc zeroes by default: no active slots, empty arrays.
    world.entities->caravans.max_count = count;
    world.entities->caravans.active_count = 0;

    world.entities->caravans.guids = memory.alloc<entity_guid>(count);
    if (world.entities->caravans.guids == nullptr)
    {
        return false;
    }

    world.entities->caravans.locations = memory.alloc<world_location>(count);
    if (world.entities->caravans.locations == nullptr)
    {
        return false;
    }

    world.entities->caravans.flags = memory.alloc<WORLD_ENTITY_CARAVAN_FLAGS>(count);
    if (world.entities->caravans.flags == nullptr)
    {
        return false;
    }
    world.entities->caravans.movements = memory.alloc<world_entity_movement>(count);
    if (world.entities->caravans.movements == nullptr)
    {
        return false;
    }

    world.entities->caravans.origin_settlements = memory.alloc<entity_guid>(count);
    if (world.entities->caravans.origin_settlements == nullptr)
    {
        return false;
    }
    world.entities->caravans.destination_settlements = memory.alloc<entity_guid>(count);
    if (world.entities->caravans.destination_settlements == nullptr)
    {
        return false;
    }

    world.entities->caravans.values = memory.alloc<ui16>(count);
    if (world.entities->caravans.values == nullptr)
    {
        return false;
    }

    return true;
}



// CARAVAN DESTINATION SELECTION


// Weight of a candidate: (potential profit (flat bonus included) per unit of cost) ^ CARAVAN_WEIGHT_EXPONENT, so better candidates are favored more than proportionally.
// destination_pool / destination_production / destination_capacity are the candidate's market goods, last daily production and capacity (settlement_market_capacity).
// The profit uses the tile distance between source and candidate, the cost is only the divisor (callers pass the tile distance, see below).
// Fixed point with 3 decimals (profit * 1000 / cost) before the power, so far candidates do not round down to 0. Zero profit = 0, zero cost counts as 1.
// Overflow: the ratio is clamped to 2^20 before the power, so a weight is at most 2^40 (exponent 2) and the sum over 4096 candidates at most 2^52.
// A higher exponent needs a lower clamp (ratio ^ exponent x 4096 must stay under 2^64).
static constexpr ui32 CARAVAN_WEIGHT_EXPONENT = 2;
static constexpr ui64 CARAVAN_WEIGHT_RATIO_MAX = 1 << 20;
static ui64 caravan_candidate_weight(ui32 caravan_value, ui32 destination_pool, ui32 destination_production, ui32 destination_capacity, ui32 tile_distance, ui32 path_cost)
{
    static constexpr ui64 WEIGHT_SCALE = 1000;
    ui64 profit = trade_profit(caravan_value, destination_pool, destination_production, destination_capacity, tile_distance, GAME_PARAMETERS.trade);
    if (profit == 0) return 0;
    ui64 ratio = profit * WEIGHT_SCALE / ia_max(path_cost, (ui32)1);
    return ia_pow(ia_min(ratio, CARAVAN_WEIGHT_RATIO_MAX), CARAVAN_WEIGHT_EXPONENT);
}

bool world_caravan_select_destination(match_world_state& world, world_location source, entity_guid origin_exclude, ui32 caravan_value,
    WORLD_ENTITY_CARAVAN_FLAGS caravan_flags, entity_guid& out_destination, ui32& out_path_cost, entity_guid destination_exclude)
{
    const world_entity_settlements& settlements = world.entities->settlements;

    // @TODO(Marc): Restore path-based target selection for a more accurate travel cost assessment, and to be able to add ships.

    // Weight of each settlement slot, 0 = not a candidate.
    // Macro entity indices are 12 bits, so this covers every settlement slot.
    ui64 weights[4096] = {};
    ASSERT(settlements.max_count <= 4096);

    // Pass 1: weigh candidates.
    ui64 totalWeight = 0;
    for (ui16 index = 0; index < settlements.max_count; index++)
    {
        entity_guid guid = settlements.guids[index];
        if (!guid.is_valid() || guid == origin_exclude || guid == destination_exclude) continue;

        ui32 tradeDist = trade_tile_distance(source, settlements.locations[index]);

        // Cost is linear in the tile distance: placeholder until the real path cost is usable again.
        weights[index] = caravan_candidate_weight(caravan_value, settlements.trade[index].market_goods,
                settlements.economies[index].production, settlement_market_capacity(settlements, index), tradeDist, tradeDist);

        totalWeight += weights[index];
    }

    if (totalWeight == 0) return false; // No candidate (or none with a profit): no random number drawn.

    // Pick: a random point in [0, totalWeight).
    // 32 random bits are enough for most totals, a second draw extends it to 64 bits (modulo bias is negligible).
    ui64 range = totalWeight;
    ui64 roll = world.rand_gen.next<ui32>();
    if (range > 0xFFFFFFFFull) roll = (roll << 32) | world.rand_gen.next<ui32>();
    roll %= range;

    // Pass 2: find the candidate holding the rolled point, using the weights stored in pass 1.
    for (ui16 index = 0; index < settlements.max_count; index++)
    {
        if (weights[index] == 0) continue;

        ui64 weight = weights[index];
        if (roll < weight)
        {
            out_destination = settlements.guids[index];
            out_path_cost = trade_tile_distance(source, settlements.locations[index]); // Tile distance, not a path cost (callers ignore it).
            return true;
        }
        roll -= weight;
    }
    ASSERT(false); // Unreachable: roll < range.
    return false;
}
