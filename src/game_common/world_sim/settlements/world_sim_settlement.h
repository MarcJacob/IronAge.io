// Economy symbols shared between world simulation themes.

#ifndef WORLD_SIM_ECONOMY_INCLUDED
#define WORLD_SIM_ECONOMY_INCLUDED

#include "core/std_types.h"
#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world_entities.h"
#include "../armies/world_sim_army.h"

// How much population "Fills" one tile of terrain.
// If population is higher than this, the settlement gets pressure to expand.
static const ui8 POP_CAP_PER_TILE = 2;

// Spawns a new settlement in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_settlement(match_world_state& world, const world_entity_settlements::single& start_state);
void world_destroy_settlement(match_world_state& world, entity_guid settlement);

// Allocates the settlements' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_settlements(match_world_state& world, mem_arena& memory, ui32 count);

// Gives the settlement to a new owner. Its tax rate resets to 0 (local wealth stays with the settlement).
void world_settlement_change_owner(match_world_state& world, ui16 settlement_index, match_player_id new_owner);

// SETTLEMENT POPULATION PRIMITIVES
// Every change to a settlement's peasants / home / raised counts and population_total goes through these (taking the settlement's index).
// They keep population_total = peasants + home + raised, never underflow (they return what was actually moved) and do not touch the player stats.
// Troop types are the TROOP_TYPE_* of world_sim_army.h. Levies have no home pool (home.levies stays 0).

// Births. Adds peasants.
static inline void settlement_add_peasants(match_world_state& world, ui16 settlement_index, ui32 count)
{
	settlement_population& population = world.entities->settlements.populations[settlement_index];
	population.peasants += count;
	population.population_total += count;
}

// Deaths. Removes up to count peasants, returns how many were removed.
static inline ui32 settlement_remove_peasants(match_world_state& world, ui16 settlement_index, ui32 count)
{
	settlement_population& population = world.entities->settlements.populations[settlement_index];
	ui32 removed = ia_min(count, population.peasants);
	population.peasants -= removed;
	population.population_total -= ia_min(removed, population.population_total);
	return removed;
}

// Deaths of raised soldiers. Removes up to count raised soldiers of a type, returns how many were removed.
static inline ui16 settlement_remove_soldiers_raised(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 count)
{
	settlement_population& population = world.entities->settlements.populations[settlement_index];
	ui16& raised = troop_count_field(population.raised, troop_type);
	ui16 removed = ia_min(count, raised);
	raised -= removed;
	population.population_total -= ia_min((ui32)removed, population.population_total);
	return removed;
}

// Training. Moves up to count peasants into the home soldiers of a type (not levies). Returns how many were moved.
static inline ui16 settlement_train_soldiers(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui32 count)
{
	settlement_population& population = world.entities->settlements.populations[settlement_index];
	ui16& home = troop_count_field(population.home, troop_type);
	ui32 moved = ia_min(ia_min(count, population.peasants), (ui32)(0xFFFF - home));
	population.peasants -= moved;
	home += (ui16)moved;
	return (ui16)moved;
}

// Discharge. Moves up to count home soldiers of a type back to peasants. Returns how many were moved.
static inline ui16 settlement_discharge_soldiers(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 count)
{
	settlement_population& population = world.entities->settlements.populations[settlement_index];
	ui16& home = troop_count_field(population.home, troop_type);
	ui16 moved = ia_min(count, home);
	home -= moved;
	population.peasants += moved;
	return moved;
}

// Levy. Moves up to count peasants into raised levies. Returns how many were moved.
static inline ui16 settlement_raise_levies(match_world_state& world, ui16 settlement_index, ui32 count)
{
	settlement_population& population = world.entities->settlements.populations[settlement_index];
	ui32 moved = ia_min(ia_min(count, population.peasants), (ui32)(0xFFFF - population.raised.levies));
	population.peasants -= moved;
	population.raised.levies += (ui16)moved;
	return (ui16)moved;
}

// Raising from the local troop pool. Moves up to count home soldiers of a type (not levies) to raised. Returns how many were moved.
static inline ui16 settlement_raise_soldiers(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 count)
{
	settlement_population& population = world.entities->settlements.populations[settlement_index];
	ui16& home = troop_count_field(population.home, troop_type);
	ui16& raised = troop_count_field(population.raised, troop_type);
	ui16 moved = ia_min(ia_min(count, home), (ui16)(0xFFFF - raised));
	home -= moved;
	raised += moved;
	return moved;
}

// Standing down. Moves up to count raised soldiers of a type back to the settlement: levies become peasants, the others rejoin the home pool.
// Returns how many were moved.
static inline ui16 settlement_release_raised(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 count)
{
	settlement_population& population = world.entities->settlements.populations[settlement_index];
	ui16& raised = troop_count_field(population.raised, troop_type);
	ui16 moved = ia_min(count, raised);
	if (troop_type == TROOP_TYPE_LEVIES)
	{
		raised -= moved;
		population.peasants += moved;
		return moved;
	}
	ui16& home = troop_count_field(population.home, troop_type);
	moved = ia_min(moved, (ui16)(0xFFFF - home));
	raised -= moved;
	home += moved;
	return moved;
}

// Claims a tile in the world for the given settlement using the origin proximity value as proximity base.
// Ensure settlement's influence bounds rectangle is kept up to date !
ui16 world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index);

// Max local troops of a settlement: a share of its total population, rounded down (shares in GAME_PARAMETERS).
// Levies are not pooled: their max is the peasants that can be raised without going below raise_min_population_left.
// Future cost / upkeep hooks would plug in next to these.
static inline army_troop_count settlement_troop_max(const settlement_population& population)
{
	ui32 minLeft = GAME_PARAMETERS.raise_min_population_left;
	ui32 levyMax = population.peasants > minLeft ? population.peasants - minLeft : 0;
	return {
		(ui16)(levyMax > 0xFFFF ? 0xFFFF : levyMax), // Levies
		(ui16)((ui64)population.population_total * GAME_PARAMETERS.men_at_arms_max_per_mille / 1000),
		(ui16)((ui64)population.population_total * GAME_PARAMETERS.horsemen_max_per_mille / 1000),
		(ui16)((ui64)population.population_total * GAME_PARAMETERS.knights_max_per_mille / 1000),
	};
}

// Ceiling actually in force: the raw player-set ceiling capped by max. It applies to the total of a type (home + raised).
static inline army_troop_count settlement_troop_effective_ceilings(const settlement_population& population, const army_troop_count& raw_ceilings)
{
	army_troop_count max = settlement_troop_max(population);
	return {
		raw_ceilings.levies < max.levies ? raw_ceilings.levies : max.levies,
		raw_ceilings.men_at_arms < max.men_at_arms ? raw_ceilings.men_at_arms : max.men_at_arms,
		raw_ceilings.horsemen < max.horsemen ? raw_ceilings.horsemen : max.horsemen,
		raw_ceilings.knights < max.knights ? raw_ceilings.knights : max.knights,
	};
}



#endif // WORLD_SIM_ECONOMY_INCLUDED
