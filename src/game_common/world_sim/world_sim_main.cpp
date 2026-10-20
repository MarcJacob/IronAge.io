// Main implementation file for world simulation match subsystem.

#include "core.h"
#include "core/math/vec2.h"
#include "game_common/match/start_params.h"
#include "game_common/match/world/world.h"
#include "world_sim.h"

// Unity-include the entire world simulation code. 
#include "world_terrain.cpp"

#include "settlements/world_sim_settlement.cpp"
#include "settlements/world_system_territory.cpp"
#include "settlements/world_system_economy.cpp"

#include "armies/world_sim_army.cpp"
#include "armies/world_system_army.cpp"
#include "armies/world_system_battle.cpp"

#include "battles/world_sim_battle.cpp"

#include "builders/world_sim_builder.cpp"
#include "builders/world_system_construction.cpp"

#include "world_system_movement.cpp"
#include "world_pathing.cpp"

// GENERAL HELPERS

bool world_entity_is_valid(const match_world_state& world, entity_guid entity)
{
	if (!entity.is_valid()) return false;

	const world_entity_container* container;

	switch (entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
		container = &world.entities->settlements;
		break;
	case ENTITY_TYPE::ARMY:
		container = &world.entities->armies;
		break;
	case ENTITY_TYPE::BUILDER:
		container = &world.entities->builders;
		break;
	case ENTITY_TYPE::BATTLE:
		container = &world.entities->battles;
		break;
	default:
		return false;
	}

	// The stored GUID is zero for inactive slots, and differs in its extra bits if the slot was reused by a newer entity.
	ui16 index = entity.get_index();
	return index < container->max_count && container->guids[index] == entity;
}

world_location world_entity_get_location(const match_world_state& world, entity_guid entity)
{
	ENTITY_TYPE type = entity.get_type();

	switch (type)
	{
	case ENTITY_TYPE::SETTLEMENT:
		return world.entities->settlements.locations[entity.get_index()];
	case ENTITY_TYPE::ARMY:
		return world.entities->armies.locations[entity.get_index()];
	case ENTITY_TYPE::BUILDER:
		// Builders have no location of their own for now: they are where their origin is.
		return world_entity_get_location(world, world.entities->builders.origin_entity[entity.get_index()]);
	case ENTITY_TYPE::BATTLE:
		return world.entities->battles.states[entity.get_index()].location;
	default:
		// .. TODO(Marc): Need to be able to report error on the match / world level somehow. Provide a function pointer ?
		return INVALID_WORLD_LOCATION;
	}
}

// MAIN LIFECYCLE FUNCTIONS

match_world_state* world_state_init(mem_arena& memory, const game_match_start_params& start_params)
{
	// Estimate size:
	// - A terrain of the given size in regions
	// - N settlements per region
	// - X micro entities of each type per region.

	ui64 memoryStart = memory.allocated_count;
	ui16 regionCount = start_params.world_size_regions.x * start_params.world_size_regions.y;

	match_world_state* newWorld = memory.alloc<match_world_state>();
	if (newWorld == nullptr) goto WORLD_INIT_FAIL;

    newWorld->rand_gen = rand_generator_create_32(start_params.random_seed);

	newWorld->terrain = memory.alloc<world_terrain>();
	if (newWorld->terrain == nullptr) goto WORLD_INIT_FAIL;

	newWorld->terrain->size_regions = start_params.world_size_regions;
	newWorld->terrain->size_tiles = start_params.world_size_regions * world_terrain::REGION_SIZE;

	newWorld->terrain->regions = memory.alloc<world_terrain::region>(regionCount);
	if (newWorld->terrain->regions == nullptr) goto WORLD_INIT_FAIL;
	ia_memzero(newWorld->terrain->regions, sizeof(world_terrain::region) * regionCount);

	newWorld->entities = memory.alloc<world_entities>();
	if (newWorld->entities == nullptr) goto WORLD_INIT_FAIL;

	newWorld->player_count = start_params.player_count;
	if (newWorld->player_count > 0)
	{
		newWorld->player_states = memory.alloc<world_player_state>(newWorld->player_count);
		if (newWorld->player_states == nullptr) goto WORLD_INIT_FAIL;
		ia_memzero(newWorld->player_states, sizeof(world_player_state) * newWorld->player_count); // Players start with an empty treasury.
	}

	newWorld->path_store = world_path_store_init(memory);
	if (newWorld->path_store == nullptr) goto WORLD_INIT_FAIL;

	// Alloc entities
	{
		const ui32 settlement_count = SETTLEMENTS_PER_REGION * regionCount;
		const ui32 micro_entity_count = MICRO_ENTITIES_PER_TYPE_PER_REGION * regionCount;

		if (!world_alloc_settlements(*newWorld, memory, settlement_count)) goto WORLD_INIT_FAIL;
		if (!world_alloc_armies(*newWorld, memory, micro_entity_count)) goto WORLD_INIT_FAIL;
		if (!world_alloc_builders(*newWorld, memory, micro_entity_count)) goto WORLD_INIT_FAIL;
		if (!world_alloc_battles(*newWorld, memory, micro_entity_count)) goto WORLD_INIT_FAIL;
	}

    world_terrain_generate(*newWorld->terrain);

	return newWorld;

WORLD_INIT_FAIL:
	memory.allocated_count = memoryStart;
	return nullptr;
}

void world_state_tick(match_world_state& world)
{
    // TEMP WORLD SIMULATION CODE
    // This is some test sequential code loosely based on the game design ideas.
    // @TODO(Marc): Figure out an appropriate abstraction for systems 
    // that allows a more granular production of such systems in a way that makes them easier to multithread.

    world_system_construction(world); // Before proximity recalc, so new roads count this tick.

    world_system_economy_daily_tick(world);
    world_system_economy_troop_growth(world); // After the daily tick's growth.
    world_system_settlement_territory(world);
    world_system_settlement_territory_proximity_recalc(world);
    world_system_settlement_territory_update_pressure(world);

    world_system_entity_movement_decision(world);
    world_system_entity_movement(world);

    world_system_armies_target_reached(world);
    world_system_armies_raising(world);
    world_system_battles(world);

    // Increment world age.
    world.age++;
}
