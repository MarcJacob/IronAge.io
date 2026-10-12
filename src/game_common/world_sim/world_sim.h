// Main collection of symbols for accessing / running the world simulation systems.

#ifndef WORLD_SIM_INCLUDED
#define WORLD_SIM_INCLUDED

#include "game_common/match/start_params.h"
#include "game_common/match/world.h"

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

// Claims a tile in the world for the given settlement using the origin proximity value as proximity base.
// Ensure settlement's influence bounds rectangle is kept up to date !
void world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index);
void world_system_economy_influence_spread(match_world_state& world);

void world_system_economy_contestation(match_world_state& world);

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
