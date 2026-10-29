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

    // The origin's active count goes up with the caravan (and down in destroy, exactly once). Ships have their own count (IS_SHIP never changes after spawn).
    settlement_trading* originTrade = caravan_origin_trade(world, start_state.origin_settlement);
    if (originTrade != nullptr)
    {
        ui8& count = caravan_is_ship(start_state.flags) ? originTrade->ship_count : originTrade->caravan_count;
        if (count < 0xFF) count++;
    }

    return newCaravanGuid;
}

void world_destroy_entity_caravan(match_world_state& world, const entity_guid& caravan)
{
    ASSERT(world_entity_is_valid(world, caravan) && caravan.get_type() == ENTITY_TYPE::CARAVAN);
    world_entity_caravans& caravans = world.entities->caravans;
    ui16 index = caravan.get_index();

    // Only a valid origin is touched: a destroyed settlement (or one whose slot was reused) keeps its own count.
    settlement_trading* originTrade = caravan_origin_trade(world, caravans.origin_settlements[index]);
    if (originTrade != nullptr)
    {
        ui8& count = caravan_is_ship(caravans.flags[index]) ? originTrade->ship_count : originTrade->caravan_count;
        if (count > 0) count--;
    }

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


// Weight of a candidate: (expected exporter gain per unit of cost) ^ CARAVAN_WEIGHT_EXPONENT, so better candidates are favored more than proportionally.
// Expected exporter gain = caravan value + exporter share of trade_volume_profit (computed with the tile distance between source and candidate).
// The cost is only the divisor (callers pass the tile distance, see below).
// Fixed point with 3 decimals (gain * 1000 / cost) before the power, so far candidates do not round down to 0. Zero gain = 0, zero cost counts as 1.
// Overflow: the ratio is clamped to 2^20 before the power, so a weight is at most 2^40 (exponent 2) and the sum over 4096 candidates at most 2^52.
// A higher exponent needs a lower clamp (ratio ^ exponent x 4096 must stay under 2^64).
static constexpr ui32 CARAVAN_WEIGHT_EXPONENT = 2;
static constexpr ui64 CARAVAN_WEIGHT_RATIO_MAX = 1 << 20;
static ui64 caravan_candidate_weight(ui32 caravan_value, ui32 destination_pool, ui32 destination_capacity, ui32 origin_capacity, ui32 destination_volume_fraction,
    ui32 tile_distance, ui32 path_cost)
{
    static constexpr ui64 WEIGHT_SCALE = 1000;
    const game_trade_parameters& trade = GAME_PARAMETERS.trade;
    ui64 profit = trade_volume_profit(caravan_value, destination_pool, destination_capacity, origin_capacity, destination_volume_fraction, tile_distance, trade);
    ui64 gain = caravan_value + profit - trade_profit_receiver_share(profit, trade);
    if (gain == 0) return 0;
    ui64 ratio = gain * WEIGHT_SCALE / ia_max(path_cost, (ui32)1);
    return ia_pow(ia_min(ratio, CARAVAN_WEIGHT_RATIO_MAX), CARAVAN_WEIGHT_EXPONENT);
}

// SPAWN SCORE (spawning mode). Score = total profit x 100 / tile distance, in thousandths (1000 = 1 percent of profit per tile).
static ui64 caravan_spawn_score(ui64 profit, ui32 tile_distance)
{
    return profit * 100000 / ia_max(tile_distance, (ui32)1);
}

// Caravan size towards a candidate: the value (at most 0xFFFF), and when spawning at most the candidate's room (capacity - both pools, floor 0).
static ui32 caravan_candidate_size(const world_entity_settlements& settlements, ui16 index, ui32 caravan_value, bool size_to_room)
{
    ui32 size = ia_min(caravan_value, (ui32)0xFFFF);
    if (!size_to_room) return size;
    ui32 total = settlement_market_total(settlements, index);
    ui32 capacity = settlement_market_capacity(settlements, index);
    return ia_min(size, capacity > total ? capacity - total : (ui32)0);
}

bool world_caravan_select_destination(match_world_state& world, world_location source, entity_guid origin, ui32 caravan_value, ui32 min_value, bool size_to_room,
    WORLD_ENTITY_CARAVAN_FLAGS caravan_flags, entity_guid& out_destination, ui32& out_value, const entity_guid* destination_excludes, ui32 destination_exclude_count)
{
    const world_entity_settlements& settlements = world.entities->settlements;

    // @TODO(Marc): Restore path-based target selection for a more accurate travel cost assessment.

    bool originValid = origin.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, origin);
    ui32 originCapacity = originValid ? settlement_market_capacity(settlements, origin.get_index()) : 0;

    // size_to_room means a spawn: threshold scoring (caravan_spawn_score) instead of the retarget weight. Needs a valid origin.
    bool spawning = size_to_room && originValid;
    ui64 minScore = (ui64)GAME_PARAMETERS.trade.min_profit_per_travel_dist_percent * 1000;

    // Weight of each settlement slot, 0 = not a candidate.
    // Macro entity indices are 12 bits, so this covers every settlement slot.
    ui64 weights[4096] = {};
    ASSERT(settlements.max_count <= 4096);

    // Pass 1: size and weigh candidates.
    ui64 totalWeight = 0;
    for (ui16 index = 0; index < settlements.max_count; index++)
    {
        entity_guid guid = settlements.guids[index];
        if (!guid.is_valid() || guid == origin) continue;

        // Caravans cannot enter a besieged settlement.
        if (settlements.sieges[index].is_valid()) continue;

        bool excluded = false;
        for (ui32 i = 0; i < destination_exclude_count; i++) excluded = excluded || guid == destination_excludes[i];
        if (excluded) continue;

        // At spawn, an origin that does not allow foreign export only targets settlements of its own owner (travelling caravans retarget freely).
        if (spawning && !settlements.trade[origin.get_index()].allow_foreign_export && settlements.owners[index] != settlements.owners[origin.get_index()]) continue;

        // Ships only go to coastal settlements (they arrive at the dock tile).
        world_location dock;
        if (caravan_is_ship(caravan_flags) && !settlement_dock_tile(world, index, dock)) continue;

        ui32 size = caravan_candidate_size(settlements, index, caravan_value, size_to_room);
        if (size == 0 || size < min_value) continue;

        ui32 tradeDist = trade_tile_distance(source, settlements.locations[index]);

        // Cost is linear in the tile distance: placeholder until the real path cost is usable again.
        if (spawning)
        {
            // Eligible only at or above the threshold. The weight is the score itself.
            ui64 profit = trade_volume_profit(size, settlement_market_total(settlements, index), settlement_market_capacity(settlements, index), originCapacity,
                    settlement_trade_volume_fraction(settlements, index), tradeDist, GAME_PARAMETERS.trade);
            ui64 score = caravan_spawn_score(profit, tradeDist);
            weights[index] = score >= minScore ? score : 0;
        }
        else
        {
            weights[index] = caravan_candidate_weight(size, settlement_market_total(settlements, index), settlement_market_capacity(settlements, index), originCapacity,
                    settlement_trade_volume_fraction(settlements, index), tradeDist, tradeDist);
        }

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
            out_value = caravan_candidate_size(settlements, index, caravan_value, size_to_room);
            return true;
        }
        roll -= weight;
    }
    ASSERT(false); // Unreachable: roll < range.
    return false;
}
