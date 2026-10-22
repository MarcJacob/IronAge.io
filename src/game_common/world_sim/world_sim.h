// Main collection of symbols for accessing / running the world simulation systems.

#ifndef WORLD_SIM_INCLUDED
#define WORLD_SIM_INCLUDED

#include "game_common/match/game_parameters.h"
#include "game_common/match/start_params.h"
#include "game_common/match/world/world.h"
#include "game_common/match/world/world_entities.h"
#include "game_common/match/world/world_terrain.h"
#include "game_common/match/world/world_pathing.h"

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
// Candidates are the other active settlements with a complete path. Weight = trade_profit / path cost, picked with world.rand_gen (not advanced without candidates).
// Returns false if there is no destination, otherwise the destination and the path cost used. Pathing to every settlement is not cached.
bool world_caravan_select_destination(match_world_state& world, world_location source, entity_guid origin_exclude, ui32 caravan_value,
    WORLD_ENTITY_CARAVAN_FLAGS caravan_flags, entity_guid& out_destination, ui32& out_path_cost);

// SYSTEMS & HELPERS

// Daily: settlements roll for each missing caravan slot, investing local wealth in caravans towards a selected destination.
void world_system_caravan_spawn(match_world_state& world);

// Caravans that reached their destination settlement trade and disappear; those whose destination is gone are cancelled.
void world_system_caravan_trade(match_world_state& world);

// Gives a caravan a new destination (its current one is invalid or unreachable), selected from its origin (or its own tile if the origin is gone).
// With none available it returns home (flag RETURNING_HOME) if the origin is valid, otherwise it is destroyed and its value is lost.
void world_caravan_retarget(match_world_state& world, entity_guid caravan);

// Advances all construction projects (workers) by one tick, destroying the finished ones.
void world_system_worker_construction(match_world_state& world);

// Makes an entity stop where it is, dropping its path and the step it was making.
static inline void world_movement_stop(world_entity_movement& movement, world_location location)
{
    movement.destination = location;
    movement.move_target = location;
    movement.path_handle = INVALID_PATH_HANDLE;
}

void world_system_settlement_territory_proximity_recalc(match_world_state& world);
void world_system_settlement_territory(match_world_state& world);

void world_system_settlement_territory_update_pressure(match_world_state& world);

// Daily, per settlement: output -> wealth, tax, consumption, population growth. See world_system_population.cpp.
void world_system_economy_daily_tick(match_world_state& world);

void world_system_economy_troop_growth(match_world_state& world);

// Daily: runs the on_daily_tick event of every building type a settlement has at least one unit of.
void world_system_settlement_buildings_daily_tick(match_world_state& world);

// Moves the entity along the way to its destination. When it stands on its move target, it picks the next one from a path.
// Returns false when there is no path to follow (the entity stays), true otherwise (moved, or already arrived).
// out_partial (optional) is set to true when the path found only gets partway to the destination (the entity still follows it).
bool world_entity_follow_path(match_world_state& world, world_entity_movement& movement, world_location& location, bool* out_partial = nullptr);

// Movement decision AI for entities. @TODO: Too centralized. Have an equivalent decision function for each entity type.
void world_system_entity_movement_decision(match_world_state& world);

// Movement application for entities that can move.
void world_system_entity_movement(match_world_state& world);

void world_system_armies_target_reached(match_world_state& world);

// Daily: every army costs upkeep per troop, paid by its owner's treasury (down to 0, no debt). Adds to the player's expenses.
void world_system_armies_upkeep(match_world_state& world);

// Raising armies standing on their destination (and not in a battle) draw men from their home settlement.
void world_system_armies_raising(match_world_state& world);

void world_system_battles(match_world_state& world);

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
