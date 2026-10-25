// Main collection of symbols for accessing / running the world simulation systems.

#ifndef WORLD_SIM_INCLUDED
#define WORLD_SIM_INCLUDED

#include "game_common/match/game_parameters.h"
#include "game_common/match/start_params.h"
#include "game_common/match/world/world.h"
#include "game_common/match/world/world_entities.h"
#include "game_common/match/world/world_terrain.h"
#include "game_common/match/world/world_pathing.h"

#include "systems/systems_economy.h"
#include "systems/systems_war.h"
#include "systems/systems_workers.h"
#include "systems/systems_world.h"

// WORLD SIM PARAMETERS & HELPERS

// True on the first tick of each day (GAME_PARAMETERS.ticks_per_day ticks).
static inline bool world_is_daily_tick(const match_world_state& world)
{
    return world.age % GAME_PARAMETERS.ticks_per_day == 0;
}

// Distances are measured with chamfer weights (integer approximation of euclidean distance, ratio 7 / 5 = 1.4 ~ sqrt(2)).
// Stepping to a tile adds its travel cost times the weight.
static constexpr ui16 PROXIMITY_WEIGHT_ORTHOGONAL = 5;
static constexpr ui16 PROXIMITY_WEIGHT_DIAGONAL = 7;

// Neighbour offsets, non-diagonal only or all 8.
static const vec2<i8> NEIGHBOUR_OFFSETS_ORTHOGONAL[] = { {-1, 0}, {0, -1}, {0, 1}, {1, 0} };
static const vec2<i8> NEIGHBOUR_OFFSETS_ALL[] = { {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1} };

// PATH STORE

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

// ENTITIES
//
// Destroys an army and returns its men to the settlement: levies become peasants, the other types rejoin the local troops (raised count down, home count up).
void world_disband_army(match_world_state& world, entity_guid army, entity_guid settlement);

// Picks a destination settlement for a caravan of the given value travelling from source (any location), excluding origin_exclude (may be INVALID_ENTITY_GUID).
// Candidates are the other active settlements (no reachability check) except destination_exclude (optional, e.g. the destination that just failed).
// Weight = (trade_profit x 1000 / tile distance) ^ 2 (see the weight helper), picked at random with world.rand_gen (not advanced without candidates).
// Returns false if there is no destination, otherwise the destination and its tile distance from source.
bool world_caravan_select_destination(match_world_state& world, world_location source, entity_guid origin_exclude, ui32 caravan_value,
    WORLD_ENTITY_CARAVAN_FLAGS caravan_flags, entity_guid& out_destination, ui32& out_path_cost, entity_guid destination_exclude = INVALID_ENTITY_GUID);

// Gives a caravan a new destination (its current one is invalid or unreachable), selected from its origin (or its own tile if the origin is gone).
// With none available it returns home (flag RETURNING_HOME) if the origin is valid, otherwise it is destroyed and its value is lost.
void world_caravan_retarget(match_world_state& world, entity_guid caravan);

// SHARED SYSTEM HELPERS

// Makes an entity stop where it is, dropping its path and the step it was making.
static inline void world_movement_stop(world_entity_movement& movement, world_location location)
{
    movement.destination = location;
    movement.move_target = location;
    movement.path_handle = INVALID_PATH_HANDLE;
}

// Moves the entity along the way to its destination. When it stands on its move target, it picks the next one from a path.
// Returns false when there is no path to follow (the entity stays), true otherwise (moved, or already arrived).
// out_partial (optional) is set to true when the path found only gets partway to the destination (the entity still follows it).
bool world_entity_follow_path(match_world_state& world, world_entity_movement& movement, world_location& location, bool* out_partial = nullptr);

// LIFECYCLE

// Generates the terrain of a world whose terrain size and regions are already set up.
void world_terrain_generate(world_terrain& terrain);
void world_terrain_init_tile_status(world_terrain& terrain); // For terrain whose tile types were set by something else (map generation).

// Initializes a new world state into the given memory from match start parameters.
match_world_state* world_state_init(mem_arena& memory, const game_match_start_params& start_params);

// Recomputes the players' settlement_count, total_population and total_troops from the settlements they own.
// Called at the end of world init and of every tick. The other player stats are maintained by the tick itself.
void world_update_player_stats(match_world_state& world);

// Advances the world state by one tick.
void world_state_tick(match_world_state& world);

#endif // WORLD_SIM_INCLUDED
