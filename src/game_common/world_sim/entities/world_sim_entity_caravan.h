// Main declaration files for symbols related to Trade Caravan entities.

#ifndef WORLD_SIM_ENTITY_CARAVAN_INCLUDED
#define WORLD_SIM_ENTITY_CARAVAN_INCLUDED

#include "core.h"

#include "../world_sim.h"
#include "game_common/match/world/world.h"
#include "game_common/match/world/world_entities.h"

entity_guid world_spawn_entity_caravan(match_world_state& world, const world_entity_caravan& start_state);
void world_destroy_entity_caravan(match_world_state& world, const entity_guid& caravan);

// Allocates the caravans' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_caravans(match_world_state& world, mem_arena& memory, ui32 count);

#endif // WORLD_SIM_ENTITY_CARAVAN_INCLUDED
