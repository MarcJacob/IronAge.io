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
#include "systems/world_system_army_standing_orders.cpp"
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
		return world.entities->engagements.locations[entity.get_index()];
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

// Stub until diplomacy: friendly = the same player, hostile = any other, neutral = never.
PLAYER_DIPLOMATIC_STATUS world_player_get_diplomatic_status(const match_world_state& world, match_player_id player, match_player_id other_player)
{
	return other_player == player ? PLAYER_DIPLOMATIC_STATUS_FRIENDLY : PLAYER_DIPLOMATIC_STATUS_HOSTILE;
}

PLAYER_DIPLOMATIC_STATUS world_entity_get_diplomatic_status(const match_world_state& world, entity_guid entity, match_player_id player)
{
	if (!world_entity_is_valid(world, entity)) return PLAYER_DIPLOMATIC_STATUS_NEUTRAL;

	if (entity.get_type() == ENTITY_TYPE::CARAVAN)
	{
		// Hostile only if both ends are. Otherwise the origin's status, except a hostile origin with a non-hostile destination: neutral (not hostile, not friendly).
		const world_entity_caravans& caravans = world.entities->caravans;
		PLAYER_DIPLOMATIC_STATUS origin = world_entity_get_diplomatic_status(world, caravans.origin_settlements[entity.get_index()], player);
		if (origin != PLAYER_DIPLOMATIC_STATUS_HOSTILE) return origin;
		bool destinationHostile = world_entity_get_diplomatic_status(world, caravans.destinations[entity.get_index()], player) == PLAYER_DIPLOMATIC_STATUS_HOSTILE;
		return destinationHostile ? PLAYER_DIPLOMATIC_STATUS_HOSTILE : PLAYER_DIPLOMATIC_STATUS_NEUTRAL;
	}
	if (entity.get_type() == ENTITY_TYPE::ENGAGEMENT) return engagement_diplomatic_status(world, entity, player);
	return world_player_get_diplomatic_status(world, player, world_entity_get_owner(world, entity));
}

bool world_pick_random_point(match_world_state& world, const world_random_point_params& params, world_location& out_point)
{
	// Not implemented yet: asserted unset so they cannot be silently ignored.
	ASSERT(params.terrain_type_mask == 0 && !params.restrict_to_player_territory && params.area_radius_tiles == 0);

	const world_dimensions& size = world.terrain->size_tiles;

	if (!params.settlement_territory.is_valid())
	{
		// No restriction: any tile of the world.
		out_point = { (ui16)(world.rand_gen.next<ui32>() % size.x), (ui16)(world.rand_gen.next<ui32>() % size.y) };
		return true;
	}

	// Settlement territory: random tiles of the rectangle bounding its claimed tiles, until one is claimed.
	ASSERT(params.settlement_territory.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, params.settlement_territory));
	ui16 settlementIndex = params.settlement_territory.get_index();
	const world_entity_settlements::territory& territory = world.entities->settlements.territories[settlementIndex];
	ui32 width = (ui32)territory.bounds_max.x - territory.bounds_min.x + 1;
	ui32 height = (ui32)territory.bounds_max.y - territory.bounds_min.y + 1;

	ui8 attempts = params.max_attempts != 0 ? params.max_attempts : 8;
	for (ui8 attempt = 0; attempt < attempts; attempt++)
	{
		world_location point = { (ui16)(territory.bounds_min.x + world.rand_gen.next<ui32>() % width), (ui16)(territory.bounds_min.y + world.rand_gen.next<ui32>() % height) };
		if (world_tile_owned_by_settlement(world, point, settlementIndex))
		{
			out_point = point;
			return true;
		}
	}

	out_point = world.entities->settlements.locations[settlementIndex];
	return false;
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

    world_system_raid_loot(world); // Before the daily economy: loots last day's production figure, and what it takes is not credited by the economy tick.
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

    world_system_army_orders(world); // Before movement: sets each army's destination from its engagement or standing order, and resolves arrivals.
    world_system_entity_movement_decision(world);
    world_system_entity_movement(world);
    world_system_caravan_trade(world); // After movement: caravans that just arrived trade this tick.

    world_system_armies_raising(world);
    world_system_engagement_auto_join(world); // After movement: armies close enough to an engagement join it.
    world_system_engagements(world);

    world_update_player_stats(world);

    // Increment world age.
    world.age++;
}
