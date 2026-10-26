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

// Makes an entity stop where it is, dropping its path and the step it was making.
static inline void world_movement_stop(world_entity_movement& movement, world_location location)
{
    movement.destination = location;
    movement.move_target = location;
    movement.path_destination = location;
    movement.path_handle = INVALID_PATH_HANDLE;
}

// Moves the entity one step in a straight line towards its current waypoint (move_target), at travel_speed divided by the cost of the tile it stands on.
// It picks a new waypoint from a path when it reaches the waypoint or when its destination changed. Arrived once its tile is the destination tile.
// Returns false when there is no path to follow (the entity stays), true otherwise (moved, or already arrived).
// params: what the entity can cross (see world_system_movement.cpp).
// out_partial (optional) is set to true when the path found only gets partway to the destination (the entity still follows it).
bool world_entity_follow_path(match_world_state& world, world_entity_movement& movement, world_location& location, const path_params& params, bool* out_partial = nullptr);

// What a path request to the world's path store gives back: enough to follow the path without holding the path itself.
struct path_result
{
    world_location next_point; // First waypoint after the start: the point to move straight towards.
    ui16 count; // Number of waypoints. 0 = nothing to do (start == end, invalid end) or no progress possible.
    ui16 length; // Total length in tiles.
    bool partial; // The path ends at the closest reachable tile instead of at the requested target.
    ui32 cost; // Total path cost (see path::cost). 0 if partial or if start == end.
    ui32 handle; // Identifies the stored path for later re-use. INVALID_PATH_HANDLE = none (the store doesn't keep paths yet).
};

// Size of the store's search memory. Fixed, as it limits how far a search goes and so must be the same on every machine.
static constexpr ui64 WORLD_PATH_STORE_SCRATCH_SIZE = KiB(256);
static constexpr ui16 WORLD_PATH_STORE_MAX_WAYPOINTS = 256;

// The world's path store: for now only holds the last path calculated, which each request overwrites.
struct world_path_store
{
    mem_arena scratch; // Search memory, cleared by every request.
    world_location points[WORLD_PATH_STORE_MAX_WAYPOINTS]; // Waypoints of the last path.
    path last; // The last path, its points being the array above.
};

// Allocates and initializes a path store from the given memory. Returns nullptr if there isn't enough.
world_path_store* world_path_store_init(mem_arena& memory);

// Requests a path from the world's path store, which may compute it or re-use a stored one. Simulation code only.
// out_result.count is 0 if there is nothing to follow.
void world_request_path(match_world_state& world, world_location start, world_location end, const path_params& params, path_result& out_result);

#endif // WORLD_MOVEMENT_INCLUDED
