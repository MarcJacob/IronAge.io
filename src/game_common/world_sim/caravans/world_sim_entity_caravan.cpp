// Main implementation file for symbols related to caravan entities.

#include "world_sim_entity_caravan.h"
#include "game_common/match/world/world_entities.h"

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

    return newCaravanGuid;
}

void world_destroy_entity_caravan(match_world_state& world, const entity_guid& caravan)
{
    ASSERT(world_entity_is_valid(world, caravan) && caravan.get_type() == ENTITY_TYPE::CARAVAN);
    world_entity_caravans& caravans = world.entities->caravans;

    caravans.guids[caravan.get_index()] = INVALID_ENTITY_GUID;
    caravans.active_count--;
}

// Allocates the caravans' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_caravans(match_world_state& world, mem_arena& memory, ui32 count)
{
    world.entities->caravans.guids = memory.alloc<entity_guid>(count);

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


