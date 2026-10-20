// Economy symbols shared between world simulation themes.

#ifndef WORLD_SIM_ECONOMY_INCLUDED
#define WORLD_SIM_ECONOMY_INCLUDED

#include "core/std_types.h"
#include "game_common/match/world/world_entities.h"

// How much population "Fills" one tile of terrain.
// If population is higher than this, the settlement gets pressure to expand.
static const ui8 POP_CAP_PER_TILE = 2;

// Raising levies never takes a settlement's population below this.
static constexpr ui32 RAISE_MIN_POPULATION_LEFT = 10;

// Spawns a new settlement in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_settlement(match_world_state& world, const world_entity_settlements::single& start_state);
void world_destroy_settlement(match_world_state& world, entity_guid settlement);

// Allocates the settlements' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_settlements(match_world_state& world, mem_arena& memory, ui32 count);

// Gives the settlement to a new owner. Its tax rate resets to 0 (local wealth stays with the settlement).
void world_settlement_change_owner(match_world_state& world, ui16 settlement_index, match_player_id new_owner);

// Removes population from a settlement's peasants, destroying the settlement if no population is left.
void world_settlement_decrease_population(match_world_state& world, entity_guid settlement, ui32 amount);

// Claims a tile in the world for the given settlement using the origin proximity value as proximity base.
// Ensure settlement's influence bounds rectangle is kept up to date !
ui16 world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index);

// Max local troops of a settlement: a share of its total population, rounded down (men at arms 2%, horsemen 1%, knights 0.5%).
// Levies are not pooled: their max is the peasants that can be raised without going below RAISE_MIN_POPULATION_LEFT.
// Future cost / upkeep hooks would plug in next to these.
static inline army_troop_count settlement_troop_max(const settlement_population& population)
{
	ui32 levyMax = population.peasants > RAISE_MIN_POPULATION_LEFT ? population.peasants - RAISE_MIN_POPULATION_LEFT : 0;
	return {
		(ui16)(levyMax > 0xFFFF ? 0xFFFF : levyMax), // Levies
		(ui16)((ui64)population.population_total * 2 / 100),     // Men at arms
		(ui16)((ui64)population.population_total * 1 / 100),     // Horsemen
		(ui16)((ui64)population.population_total * 5 / 1000),    // Knights
	};
}

// Ceiling actually in force: the raw player-set ceiling capped by max.
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
