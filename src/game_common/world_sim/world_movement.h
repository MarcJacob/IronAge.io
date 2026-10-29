// Symbols for moving entities around.

#ifndef WORLD_MOVEMENT_INCLUDED
#define WORLD_MOVEMENT_INCLUDED

#include "core.h"

#include "game_common/match/world/world_terrain.h"
#include "game_common/match/world/world_pathing.h"

// TERRAIN TRAVEL

// Travel difficulty over each terrain type. Used for tile travel distance and pathfinding.
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
    ui8 passableTerrain = 0;
    if (params.mode == PATH_MODE_SEA) passableTerrain = PATH_TERRAIN_WATER;
    else if (params.mode != PATH_MODE_ANY) passableTerrain = PATH_TERRAIN_LAND;
    if (passableTerrain != 0 && (passableTerrain & (1 << (ui8)type)) == 0) return 0;

    // Roads return the road travel cost no matter what.
    if (region.status[tile_index].is_road) return ROAD_TRAVEL_COST;

    // Stop here if we can only path on roads.
    if (params.mode == PATH_MODE_LAND_ROADS_ONLY) return 0;

    // If a valid "within settlement territory" guid was given, prevent pathing through any tile that isn't in the settlement.
    if (params.within_settlement_territory.is_valid() 
            && params.within_settlement_territory.get_index() != terrain.regions[region_index].owners[tile_index]) return 0;

    // Default case: "native" terrain travel cost.
    return TILE_TERRAIN_TRAVEL_COST[(ui8)type];
}

// Makes an entity stop where it is, dropping its path and the step it was making.
static inline void world_movement_stop(world_entity_movement& movement, world_location location)
{
    movement.destination = location;
    movement.path_handle = INVALID_PATH_HANDLE;
}

// Moves the entity one step in a straight line towards the center of the given target tile, at travel_speed divided by the cost of the tile it stands on.
// Returns false, without moving, if the step would enter a tile that is impassable for params (see world_system_movement.cpp).
bool world_entity_step_towards(const match_world_state& world, world_entity_movement& movement, world_location& location, world_location target, const path_params& params);

// Size of the store's search memory. Fixed, as it limits how far a search goes and so must be the same on every machine.
static constexpr ui64 WORLD_PATH_STORE_SCRATCH_SIZE = KiB(256);

// The world's path store. SKELETON: only the search memory for now. The three functions below are the only ones the rest of the simulation uses.
struct world_path_store
{
    mem_arena scratch; // Search memory.
};

// Allocates and initializes a path store from the given memory. Returns nullptr if there isn't enough.
world_path_store* world_path_store_init(mem_arena& memory);

// (a) Gets a path from start to end for the given mode. Returns a handle to it, INVALID_PATH_HANDLE if there is none.
ui32 world_path_store_get_path(match_world_state& world, world_location start, world_location end, PATH_MODE mode);

// (b) Entity movement: moves the entity along its path (movement.path_handle) towards movement.destination, updating its tile and sub-tile location.
// Returns true while it is moving / has a path, false when movement is over (arrived) or impossible.
bool world_path_store_move_entity(match_world_state& world, world_entity_movement& movement, world_location& location);

// (c) Returns the tile following "from" on the path path_handle towards destination, or "from" if there is none. Used by road construction.
world_location world_path_store_next_tile(match_world_state& world, ui32 path_handle, world_location from, world_location destination);

#endif // WORLD_MOVEMENT_INCLUDED
