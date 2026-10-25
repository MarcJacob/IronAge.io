// Symbols for moving entities around.

#ifndef WORLD_MOVEMENT_INCLUDED
#define WORLD_MOVEMENT_INCLUDED

#include "core.h"

#include "game_common/match/world/world_terrain.h"
#include "game_common/match/world/world_pathing.h"

// TERRAIN TRAVEL

// Travel difficulty over each terrain type. Used for tile proximity and pathfinding.
static constexpr ui8 TILE_TERRAIN_TRAVEL_COST[] = {
    5, // LAND_PLAINS
    7, // LAND_HILLS
    12, // LAND_MOUNTAINS

    3, // WATER_COAST
    7, // WATER_SEA
};

// Travel difficulty of a road tile, whatever its terrain.
static constexpr ui8 ROAD_TRAVEL_COST = 1;

// Returns the travel cost of entering a tile for the given path parameters. 0 means impassable / invalid for the given params.
static inline ui8 world_tile_move_cost(const world_terrain& terrain, ui16 region_index, ui16 tile_index, const path_params& params)
{
    const world_terrain::region& region = terrain.regions[region_index];
    TERRAIN_TILE_TYPE type = region.terrain_types[tile_index];

    // Check terrain flags. If all flags are at 0, then all terrains are allowed.
    if (params.passable_terrain != 0 && (params.passable_terrain & (1 << (ui8)type)) == 0) return 0;

    // Roads return the road travel cost no matter what.
    if (region.status[tile_index].is_road) return ROAD_TRAVEL_COST;

    // Stop here if we can only path on roads.
    if (params.roads_only) return 0;

    // If a valid "within settlement territory" guid was given, prevent pathing through any tile that isn't in the settlement.
    if (params.within_settlement_territory.is_valid() 
            && params.within_settlement_territory.get_index() != terrain.regions[region_index].owners[tile_index]) return 0;

    // Default case: "native" terrain travel cost.
    return TILE_TERRAIN_TRAVEL_COST[(ui8)type];
}


#endif // WORLD_MOVEMENT_INCLUDED
