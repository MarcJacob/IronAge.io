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

// Destroys an army and returns its men to the settlement: levies become peasants, the other types rejoin the local troops (raised count down, home count up).
void world_disband_army(match_world_state& world, entity_guid army, entity_guid settlement);

// Picks a destination settlement for a caravan travelling from source (any location), excluding origin (may be INVALID_ENTITY_GUID; its capacity scales the profit).
// Candidates are the other active settlements (no reachability check) except destination_excludes (optional, e.g. destinations that just failed).
// Caravan size per candidate: caravan_value (at most 0xFFFF), with size_to_room at most the candidate's room (capacity - pool). Sizes below min_value are not candidates.
// Weight = ((size + exporter share of trade_volume_profit) x 1000 / tile distance) ^ 2 (see the weight helper), picked at random with world.rand_gen
// (not advanced without candidates). With size_to_room (a spawn, origin valid) it is instead the spawn score (home scarcity included), only candidates
// reaching trade.min_profit_per_travel_dist_percent count (see caravan_spawn_score). Returns false if there is no destination, otherwise the destination and the caravan size for it.
bool world_caravan_select_destination(match_world_state& world, world_location source, entity_guid origin, ui32 caravan_value, ui32 min_value, bool size_to_room,
    WORLD_ENTITY_CARAVAN_FLAGS caravan_flags, entity_guid& out_destination, ui32& out_value,
    const entity_guid* destination_excludes = nullptr, ui32 destination_exclude_count = 0);

// Gives a caravan a new destination (its current one is invalid or unreachable), selected from its origin (or its own tile if the origin is gone).
// With none available it returns home (flag RETURNING_HOME) if the origin is valid, otherwise it is destroyed and its value is lost.
void world_caravan_retarget(match_world_state& world, entity_guid caravan);

// SHARED SYSTEM HELPERS

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
