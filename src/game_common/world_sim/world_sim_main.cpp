// Main implementation file for world simulation match subsystem.

#include "core.h"
#include "core/math/vec2.h"
#include "game_common/match/start_params.h"
#include "game_common/match/world/world.h"
#include "world_sim.h"

// Unity-include the entire world simulation code. 
#include "../map_gen/map_gen_main.cpp"
#include "world_terrain.cpp"

// Entities.
#include "entities/world_sim_entity_settlement.cpp"
#include "entities/world_sim_entity_army.cpp"
#include "entities/world_sim_entity_engagement.cpp"
#include "entities/world_sim_entity_worker.cpp"
#include "entities/world_sim_entity_caravan.cpp"

// Systems.
#include "systems/world_system_settlement_territory.cpp"
#include "systems/world_system_settlement_economy.cpp"
#include "systems/world_system_army.cpp"
#include "systems/world_system_army_economy.cpp"
#include "systems/world_system_standing_orders.cpp"
#include "systems/world_system_engagement.cpp"
#include "systems/world_system_worker_construction.cpp"
#include "systems/world_system_caravan_spawn.cpp"
#include "systems/world_system_caravan_trade.cpp"
#include "systems/world_system_movement.cpp"

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
	case ENTITY_TYPE::WORKER:
		container = &world.entities->workers;
		break;
	case ENTITY_TYPE::ENGAGEMENT:
		container = &world.entities->engagements;
		break;
    case ENTITY_TYPE::CARAVAN:
        container = &world.entities->caravans;
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
	case ENTITY_TYPE::WORKER:
		return world.entities->workers.locations[entity.get_index()];
	case ENTITY_TYPE::ENGAGEMENT:
		return world.entities->engagements.states[entity.get_index()].location;
    case ENTITY_TYPE::CARAVAN:
        return world.entities->caravans.locations[entity.get_index()];
	default:
		// .. TODO(Marc): Need to be able to report error on the match / world level somehow. Provide a function pointer ?
		return INVALID_WORLD_LOCATION;
	}
}

match_player_id world_entity_get_owner(const match_world_state& world, entity_guid entity)
{
	if (!world_entity_is_valid(world, entity)) return INVALID_MATCH_PLAYER_ID;

	switch (entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
		return world.entities->settlements.owners[entity.get_index()];
	case ENTITY_TYPE::ARMY:
		return world.entities->armies.owners[entity.get_index()];
	case ENTITY_TYPE::WORKER:
		// Owner of the origin entity, whatever its type.
		return world_entity_get_owner(world, world.entities->workers.origin_entity[entity.get_index()]);
	case ENTITY_TYPE::CARAVAN:
		return world_entity_get_owner(world, world.entities->caravans.origin_settlements[entity.get_index()]);
	default:
		return INVALID_MATCH_PLAYER_ID;
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
		if (!world_alloc_workers(*newWorld, memory, micro_entity_count)) goto WORLD_INIT_FAIL;
		if (!world_alloc_engagements(*newWorld, memory, micro_entity_count)) goto WORLD_INIT_FAIL;
        if (!world_alloc_caravans(*newWorld, memory, micro_entity_count)) goto WORLD_INIT_FAIL;
	}

    if (start_params.procedural_map)
    {
        if (!map_gen_generate(*newWorld->terrain, start_params.map_gen, newWorld->rand_gen.next<ui32>(), memory)) goto WORLD_INIT_FAIL;
        world_terrain_init_tile_status(*newWorld->terrain);
    }
    else
    {
        world_terrain_generate(*newWorld->terrain);
    }

	world_update_player_stats(*newWorld);

	return newWorld;

WORLD_INIT_FAIL:
	memory.allocated_count = memoryStart;
	return nullptr;
}

void world_update_player_stats(match_world_state& world)
{
    for (ui16 playerIndex = 0; playerIndex < world.player_count; playerIndex++)
    {
        world_player_state& player = world.player_states[playerIndex];
        player.settlement_count = 0;
        player.total_population = 0;
        player.total_troops = 0;
    }

    // Population and soldiers (raised ones included) belong to the settlement's owner.
    const world_entity_settlements& settlements = world.entities->settlements;
    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.owners[settlementIndex] >= world.player_count) continue;

        world_player_state& player = world.player_states[settlements.owners[settlementIndex]];
        player.settlement_count++;
        player.total_population += settlements.pop_total[settlementIndex];
        player.total_troops += settlements.soldiers_total(settlementIndex);
    }
}

void world_state_tick(match_world_state& world)
{
    // TEMP WORLD SIMULATION CODE
    // This is some test sequential code loosely based on the game design ideas.
    // @TODO(Marc): Figure out an appropriate abstraction for systems 
    // that allows a more granular production of such systems in a way that makes them easier to multithread.

    world_system_economy_daily_tick(world);
    world_system_economy_troop_growth(world); // After the daily tick's growth.
    world_system_settlement_buildings_daily_tick(world); // After everything else of the economy is resolved.
    world_system_caravan_spawn(world); // After the economy's sales and buildings set the caravan max.
    world_system_worker_construction(world); // After the daily economy (treasury is filled by tax, expenses reset), before distance recalc so new roads count this tick.
    world_system_armies_upkeep(world); // After the daily economy reset expenses.
    world_system_local_troops_pay(world);

    world_system_settlement_territory(world);
    world_system_settlement_territory_distance_recalc(world);
    world_system_settlement_territory_takeover(world); // After the distance recalc: decides from settled distances.

    world_system_army_orders(world); // Before movement: sets each army's destination from its engagement or standing order.
    world_system_entity_movement_decision(world);
    world_system_entity_movement(world);
    world_system_caravan_trade(world); // After movement: caravans that just arrived trade this tick.

    world_system_army_orders_arrival(world); // After movement: attacking armies close enough to their target resolve the attack.
    world_system_armies_raising(world);
    world_system_engagements(world);

    world_update_player_stats(world);

    // Increment world age.
    world.age++;
}
