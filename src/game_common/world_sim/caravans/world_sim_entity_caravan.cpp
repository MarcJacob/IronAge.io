// Main implementation file for symbols related to caravan entities.

#include "world_sim_entity_caravan.h"
#include "game_common/match/world/world_entities.h"

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

    // Apply start state

    caravans.locations[freeIndex] = start_state.location;
    caravans.flags[freeIndex] = start_state.flags;
    caravans.movements[freeIndex] = start_state.movement;

    caravans.origin_settlements[freeIndex] = start_state.origin_settlement;
    caravans.destination_settlements[freeIndex] = start_state.destination_settlement;

    caravans.values[freeIndex] = start_state.value;

    // The origin's active count goes up with the caravan (and down in destroy, exactly once).
    settlement_trading* originTrade = caravan_origin_trade(world, start_state.origin_settlement);
    if (originTrade != nullptr && originTrade->caravan_count < 0xFF) originTrade->caravan_count++;

    return newCaravanGuid;
}

void world_destroy_entity_caravan(match_world_state& world, const entity_guid& caravan)
{
    ASSERT(world_entity_is_valid(world, caravan) && caravan.get_type() == ENTITY_TYPE::CARAVAN);
    world_entity_caravans& caravans = world.entities->caravans;

    // Only a valid origin is touched: a destroyed settlement (or one whose slot was reused) keeps its own count.
    settlement_trading* originTrade = caravan_origin_trade(world, caravans.origin_settlements[caravan.get_index()]);
    if (originTrade != nullptr && originTrade->caravan_count > 0) originTrade->caravan_count--;

    caravans.guids[caravan.get_index()] = INVALID_ENTITY_GUID;
    caravans.active_count--;
}

bool world_entity_query_caravan(const match_world_state& world, entity_guid entity, world_entity_caravan& out_state)
{
    if (entity.get_type() != ENTITY_TYPE::CARAVAN || !world_entity_is_valid(world, entity)) return false;

    const world_entity_caravans& caravans = world.entities->caravans;
    ui16 i = entity.get_index();

    out_state = {
        .location = caravans.locations[i],
        .movement = caravans.movements[i],
        .flags = caravans.flags[i],
        .origin_settlement = caravans.origin_settlements[i],
        .destination_settlement = caravans.destination_settlements[i],
        .value = caravans.values[i],
    };
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


