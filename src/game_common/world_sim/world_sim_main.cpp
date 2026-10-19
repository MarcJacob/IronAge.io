// Main implementation file for world simulation match subsystem.

#include "core.h"
#include "core/math/vec2.h"
#include "game_common/match/start_params.h"
#include "game_common/match/world/world.h"
#include "world_sim.h"

// Unity-include the entire world simulation code. 
#include "world_entities.cpp"
#include "world_terrain.cpp"
#include "settlements/world_sim_settlement.cpp"
#include "settlements/world_system_territory.cpp"
#include "settlements/world_system_population.cpp"
#include "armies/world_sim_army.cpp"
#include "armies/world_system_army.cpp"
#include "armies/world_system_battle.cpp"
#include "battles/world_sim_battle.cpp"
#include "builders/world_sim_builder.cpp"
#include "builders/world_system_construction.cpp"
#include "world_system_movement.cpp"
#include "world_pathing.cpp"

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

    world_system_economy_pop_growth(world);
    world_system_economy_troop_growth(world);
    world_system_settlement_territory(world);
    world_system_economy_proximity_recalc(world);
    world_system_economy_update_pressure(world);

    world_system_entity_movement_decision(world);
    world_system_entity_movement(world);

    world_system_armies_target_reached(world);
    world_system_armies_raising(world);
    world_system_battles(world);

    // Increment world age.
    world.age++;
}
