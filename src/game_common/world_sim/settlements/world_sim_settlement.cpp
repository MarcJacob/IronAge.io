#include "world_sim_settlement.h"
#include "game_common/match/world/world_terrain.h"
#include "../world_sim.h"

entity_guid world_spawn_settlement(match_world_state& world, const world_entity_settlements::single& start_state)
{
	ui16 freeIndex = 0;
	while (freeIndex < world.entities->settlements.max_count && world.entities->settlements.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities->settlements.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::SETTLEMENT, freeIndex, world.age % 0x1000); // 12-bit extra field.

	world.entities->settlements.guids[freeIndex] = newID;
	world.entities->settlements.active_count++;

	// Apply start state.
	world.entities->settlements.owners[freeIndex] = start_state.owner;
	world.entities->settlements.locations[freeIndex] = start_state.location;
    world.entities->settlements.founding_ticks[freeIndex] = world.age;

    world.entities->settlements.base_land_outputs[freeIndex] = start_state.base_land_output;
	world.entities->settlements.local_wealth[freeIndex] = start_state.local_wealth;
	world.entities->settlements.tax_rates[freeIndex] = start_state.tax_rate;
	world.entities->settlements.finances[freeIndex] = start_state.finances; 
    world.entities->settlements.territories[freeIndex] = start_state.territory;
	world.entities->settlements.troop_ceilings[freeIndex] = start_state.troop_ceilings;

	settlement_population& population = world.entities->settlements.populations[freeIndex];
    population = start_state.population;

    // Init default values if they weren't specified by start state.
    SET_DEFAULT_VAL(population.peasants, 100);
	population.population_total = population.at_home() + population.raised.levies + population.raised.men_at_arms + population.raised.horsemen + population.raised.knights;
    SET_DEFAULT_VAL_STRUCT(world.entities->settlements.territories[freeIndex], (world_entity_settlements::territory
            {
                .bounds_min = world.entities->settlements.locations[freeIndex],
                .bounds_max = world.entities->settlements.locations[freeIndex],
                .pressure = 0,
            }));

    // Claim own tile and set it as border to begin influence field.
    ui16 regionIndex, tileIndex;
    world_get_tile(*world.terrain, start_state.location, regionIndex, tileIndex);

    world_influence_claim_tile(world, regionIndex, tileIndex, freeIndex);

	return newID;
}

void world_destroy_settlement(match_world_state& world, entity_guid settlement)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	world_entity_settlements& settlements = world.entities->settlements;

	settlements.guids[settlement.get_index()] = INVALID_ENTITY_GUID;
	settlements.active_count--;
}


void world_settlement_change_owner(match_world_state& world, ui16 settlement_index, match_player_id new_owner)
{
	world.entities->settlements.owners[settlement_index] = new_owner;
	world.entities->settlements.tax_rates[settlement_index] = 0;
}

void world_settlement_decrease_population(match_world_state& world, entity_guid settlement, ui32 amount)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	settlement_population& population = world.entities->settlements.populations[settlement.get_index()];

	ui32 removed = amount < population.peasants ? amount : population.peasants;
	population.peasants -= removed;
	population.population_total -= removed;
	if (population.population_total == 0)
	{
		world_destroy_settlement(world, settlement);
	}
}

bool world_entity_query_settlement(const match_world_state& world, entity_guid entity, world_entity_settlements::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::SETTLEMENT || !world_entity_is_valid(world, entity)) return false;

	const world_entity_settlements& settlements = world.entities->settlements;
	ui16 i = entity.get_index();

	out_state = {
		.owner = settlements.owners[i],
		.location = settlements.locations[i],
        .founding_tick = settlements.founding_ticks[i],
		.population = settlements.populations[i],
		.base_land_output = settlements.base_land_outputs[i],
		.local_wealth = settlements.local_wealth[i],
		.tax_rate = settlements.tax_rates[i],
		.finances = settlements.finances[i],
		.territory = settlements.territories[i],
		.troop_ceilings = settlements.troop_ceilings[i],
		.troop_max = settlement_troop_max(settlements.populations[i]),
		.troop_effective_ceilings = settlement_troop_effective_ceilings(settlements.populations[i], settlements.troop_ceilings[i]),
	};
	return true;
}

bool world_is_valid_settlement_location(const match_world_state& world, world_location loc, match_player_id player)
{
	const world_terrain& terrain = *world.terrain;
	if (loc.x >= terrain.size_tiles.x || loc.y >= terrain.size_tiles.y) return false;

	ui16 regionIndex, tileIndex;
	world_get_tile(terrain, loc, regionIndex, tileIndex);

	TERRAIN_TILE_TYPE type = terrain.regions[regionIndex].terrain_types[tileIndex];
	if (type != TERRAIN_TILE_TYPE::LAND_PLAINS && type != TERRAIN_TILE_TYPE::LAND_HILLS) return false;

	const world_entity_settlements& settlements = world.entities->settlements;

	// Tile owner is a settlement slot index.
	ui16 tileOwner = terrain.regions[regionIndex].owners[tileIndex];
	if (tileOwner != world_terrain::UNOWNED_TILE && settlements.owners[tileOwner] != player) return false;

	for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid()) continue;

		i32 dx = (i32)settlements.locations[settlementIndex].x - (i32)loc.x;
		i32 dy = (i32)settlements.locations[settlementIndex].y - (i32)loc.y;
		if (dx * dx + dy * dy < (i32)(SETTLEMENT_MIN_DISTANCE * SETTLEMENT_MIN_DISTANCE)) return false;
	}

	return true;
}

bool world_alloc_settlements(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_settlements& settlements = world.entities->settlements;

	settlements.max_count = count;
	settlements.active_count = 0;

	settlements.guids = memory.alloc<entity_guid>(count);
	if (settlements.guids == nullptr) return false;

	settlements.owners = memory.alloc<match_player_id>(count);
	if (settlements.owners == nullptr) return false;

	settlements.locations = memory.alloc<world_location>(count);
	if (settlements.locations == nullptr) return false;

    settlements.founding_ticks = memory.alloc<ui64>(count);
    if (settlements.founding_ticks == nullptr) return false;

	settlements.populations = memory.alloc<settlement_population>(count);
	if (settlements.populations == nullptr) return false;

	settlements.base_land_outputs = memory.alloc<ui32>(count);
	if (settlements.base_land_outputs == nullptr) return false;

	settlements.local_wealth = memory.alloc<ui32>(count);
	if (settlements.local_wealth == nullptr) return false;

	settlements.tax_rates = memory.alloc<i8>(count);
	if (settlements.tax_rates == nullptr) return false;

	settlements.finances = memory.alloc<settlement_finances>(count);
	if (settlements.finances == nullptr) return false;

	settlements.territories = memory.alloc<world_entity_settlements::territory>(count);
	if (settlements.territories == nullptr) return false;

	settlements.troop_ceilings = memory.alloc<army_troop_count>(count);
	if (settlements.troop_ceilings == nullptr) return false;

	return true;
}
