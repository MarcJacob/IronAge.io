// Main collection of symbols for accessing / running the world simulation systems.

#ifndef WORLD_SIM_INCLUDED
#define WORLD_SIM_INCLUDED

#include "game_common/match/start_params.h"
#include "game_common/match/world.h"
#include "game_common/match/pathing.h"

// TERRAIN TRAVEL

// Travel difficulty over each terrain type. Used for tile proximity and pathfinding.
static const ui8 TILE_TERRAIN_TRAVEL_COST[] = {
    3, // LAND_PLAINS
    5, // LAND_HILLS
    10, // LAND_MOUNTAINS

    1, // WATER_COAST
    4, // WATER_SEA
};

// Travel difficulty of a road tile, whatever its terrain.
static constexpr ui8 ROAD_TRAVEL_COST = 1;

// Distances are measured with chamfer weights (integer approximation of euclidean distance, ratio 7 / 5 = 1.4 ~ sqrt(2)).
// Stepping to a tile adds its travel cost times the weight.
static constexpr ui16 PROXIMITY_WEIGHT_ORTHOGONAL = 5;
static constexpr ui16 PROXIMITY_WEIGHT_DIAGONAL = 7;

// Neighbour offsets, non-diagonal only or all 8.
static const vec2<i8> NEIGHBOUR_OFFSETS_ORTHOGONAL[] = { {-1, 0}, {0, -1}, {0, 1}, {1, 0} };
static const vec2<i8> NEIGHBOUR_OFFSETS_ALL[] = { {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1} };

// Returns the travel cost of entering a tile for the given path parameters. 0 means impassable.
static inline ui8 world_tile_move_cost(const world_terrain& terrain, ui16 region_index, ui16 tile_index, const path_params& params)
{
    const world_terrain::region& region = terrain.regions[region_index];
    TERRAIN_TILE_TYPE type = region.terrain_types[tile_index];

    if ((params.passable_terrain & (1 << (ui8)type)) == 0) return 0;

    if (region.status[tile_index].is_road) return ROAD_TRAVEL_COST;
    if (params.roads_only) return 0;

    return TILE_TERRAIN_TRAVEL_COST[(ui8)type];
}

// PATH STORE

// What a path request to the world's path store gives back: enough to follow the path without holding the path itself.
struct path_result
{
    world_location next_point; // First waypoint after the start: the point to move straight towards.
    ui16 count; // Number of waypoints. 0 = nothing to do (start == end, invalid end) or no progress possible.
    ui16 length; // Total length in tiles.
    bool partial; // The path ends at the closest reachable tile instead of at the requested target.
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

// ENTITIES

// Spawns a new settlement in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_settlement(match_world_state& world, const world_entity_settlements::single& start_state);

// Spawns a new caravan in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_caravan(match_world_state& world, const world_entity_caravans::single& start_state);

// Spawns a new army in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_army(match_world_state& world, const world_entity_armies::single& start_state);

void world_destroy_settlement(match_world_state& world, entity_guid settlement);
void world_destroy_caravan(match_world_state& world, entity_guid caravan);
void world_destroy_army(match_world_state& world, entity_guid army);

void world_settlement_decrease_population(match_world_state& world, entity_guid settlement, ui32 amount);

bool world_alloc_settlements(match_world_state& world, mem_arena& memory, ui32 count);

bool world_alloc_caravans(match_world_state& world, mem_arena& memory, ui32 count);

bool world_alloc_armies(match_world_state& world, mem_arena& memory, ui32 count);

// SYSTEMS & HELPERS

// Makes an entity stop where it is, dropping its path and the step it was making.
static inline void world_movement_stop(world_entity_movement& movement, world_location location)
{
    movement.destination = location;
    movement.move_target = location;
    movement.path_handle = INVALID_PATH_HANDLE;
}

// Claims a tile in the world for the given settlement using the origin proximity value as proximity base.
// Ensure settlement's influence bounds rectangle is kept up to date !
ui16 world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index);
void world_system_economy_proximity_recalc(match_world_state& world);
void world_system_economy_influence_spread(match_world_state& world);

void world_system_economy_contestation(match_world_state& world);

void world_system_economy_update_pressure(match_world_state& world);

void world_system_economy_pop_growth(match_world_state& world);

void world_system_settlement_spawn_caravans(match_world_state& world);

void world_system_entity_movement_decision(match_world_state& world);

void world_system_entity_movement(match_world_state& world);

void world_system_caravan_arrivals(match_world_state& world);

void world_system_armies_target_reached(match_world_state& world);

// LIFECYCLE

// Initializes a new world state into the given memory from match start parameters.
match_world_state* world_state_init(mem_arena& memory, const game_match_start_params& start_params);

// Advances the world state by one tick.
void world_state_tick(match_world_state& world);

#endif // WORLD_SIM_INCLUDED
