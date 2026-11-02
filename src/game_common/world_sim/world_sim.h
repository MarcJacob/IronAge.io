// Main collection of symbols for accessing / running the world simulation systems.

#ifndef WORLD_SIM_INCLUDED
#define WORLD_SIM_INCLUDED

#include "game_common/match/game_parameters.h"
#include "game_common/match/start_params.h"
#include "game_common/match/world/world_state.h"
#include "game_common/match/world/world_terrain.h"
#include "game_common/match/world/world_pathing.h"

#include "systems/systems_economy.h"
#include "systems/systems_war.h"
#include "systems/systems_workers.h"
#include "systems/systems_world.h"

// True on the first tick of each day (GAME_PARAMETERS.ticks_per_day ticks).
static inline bool world_is_daily_tick(const match_world_state& world)
{
    return world.age % GAME_PARAMETERS.ticks_per_day == 0;
}

// ENTITIES

// SHARED SYSTEM HELPERS

// True if the tile at the location is claimed by the settlement (it is part of its territory).
static inline bool world_tile_owned_by_settlement(const match_world_state& world, world_location location, ui16 settlement_index)
{
    ui16 regionIndex, tileIndex;
    world_get_tile(*world.terrain, location, regionIndex, tileIndex);
    return world.terrain->regions[regionIndex].owners[tileIndex] == settlement_index;
}

// Restrictions of a random point of the world. Zero-initialized = no restriction at all (any tile of the world).
// Only settlement_territory is applied for now: the other restrictions are not implemented and asserted unset.
struct world_random_point_params
{
    entity_guid settlement_territory; // Only tiles claimed by this settlement. INVALID_ENTITY_GUID = any.

    // @TODO(Marc): Not implemented yet.
    ui8 terrain_type_mask; // Only tiles of these types (bit = 1 << TERRAIN_TILE_TYPE, see PATH_TERRAIN_*). 0 = any.
    bool restrict_to_player_territory; // Only tiles claimed by a settlement of player_territory.
    match_player_id player_territory;
    ui16 area_radius_tiles; // Only tiles within this many tiles of area_center. 0 = no area restriction.
    world_location area_center;

    ui8 max_attempts; // Random tiles tried before giving up on a restrictive search. 0 = default (8). Unrestricted searches always succeed in one.
};

// Picks a random tile of the world satisfying the restrictions (see world_random_point_params), using the world's deterministic random generator.
// Returns false if none was found, in which case out_point is the settlement's location (settlement territory restriction) or INVALID_WORLD_LOCATION.
bool world_pick_random_point(match_world_state& world, const world_random_point_params& params, world_location& out_point);

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
