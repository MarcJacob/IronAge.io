// Main declaration files for symbols related to Trade Caravan entities.

#ifndef WORLD_SIM_ENTITY_CARAVAN_INCLUDED
#define WORLD_SIM_ENTITY_CARAVAN_INCLUDED

#include "core.h"

#include "../world_sim.h"
#include "game_common/match/world/world_state.h"
#include "world_sim_entity_settlement.h"

// What caravans can cross: land only, straight segments. Other members zero-initialized.
static const path_params CARAVAN_PATH_PARAMS = { .passable_terrain = PATH_TERRAIN_LAND, .straight_segments = true };
// Ships (IS_SHIP): water only.
static const path_params SHIP_PATH_PARAMS = { .passable_terrain = PATH_TERRAIN_WATER, .straight_segments = true };

static inline bool caravan_is_ship(WORLD_ENTITY_CARAVAN_FLAGS flags)
{
    return ((ui8)flags & (ui8)WORLD_ENTITY_CARAVAN_FLAGS::IS_SHIP) != 0;
}

static inline const path_params& caravan_path_params(WORLD_ENTITY_CARAVAN_FLAGS flags)
{
    return caravan_is_ship(flags) ? SHIP_PATH_PARAMS : CARAVAN_PATH_PARAMS;
}

// Tile a caravan leaves from / travels to for a settlement: the settlement tile, or for ships its dock tile (settlement_dock_tile).
// A ship endpoint without a dock (should not happen: ship origins have a Trade Port, ship destinations are coastal) falls back to the settlement tile.
static inline world_location caravan_endpoint(const match_world_state& world, WORLD_ENTITY_CARAVAN_FLAGS flags, ui16 settlement_index)
{
    world_location tile = world.entities->settlements.locations[settlement_index];
    if (caravan_is_ship(flags)) settlement_dock_tile(world, settlement_index, tile);
    return tile;
}

entity_guid world_spawn_entity_caravan(match_world_state& world, const world_entity_caravan& start_state);
void world_destroy_entity_caravan(match_world_state& world, const entity_guid& caravan);

// Allocates the caravans' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_caravans(match_world_state& world, mem_arena& memory, ui32 count);

#endif // WORLD_SIM_ENTITY_CARAVAN_INCLUDED
