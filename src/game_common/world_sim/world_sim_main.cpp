// Main implementation file for world simulation match subsystem.

#include "core.h"
#include "core/math/vec2.h"
#include "game_common/match/start_params.h"
#include "game_common/match/world.h"
#include "world_sim.h"

// Unity-include the entire world simulation code. 
#include "world_entities.cpp"
#include "world_system_combat.cpp"
#include "world_system_movement.cpp"
#include "world_system_economy.cpp"

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

	newWorld->terrain.size_regions = start_params.world_size_regions;
	newWorld->terrain.size_tiles = start_params.world_size_regions * world_terrain::REGION_SIZE;

	newWorld->terrain.regions = memory.alloc<world_terrain::region>(regionCount);
	if (newWorld->terrain.regions == nullptr) goto WORLD_INIT_FAIL;
	ia_memzero(newWorld->terrain.regions, sizeof(world_terrain::region) * regionCount);

	// Alloc entities
	{
		const ui32 settlement_count = SETTLEMENTS_PER_REGION * regionCount;
		const ui32 micro_entity_count = MICRO_ENTITIES_PER_TYPE_PER_REGION * regionCount;

		if (!world_alloc_settlements(*newWorld, memory, settlement_count)) goto WORLD_INIT_FAIL;
		if (!world_alloc_caravans(*newWorld, memory, micro_entity_count)) goto WORLD_INIT_FAIL;
		if (!world_alloc_armies(*newWorld, memory, micro_entity_count)) goto WORLD_INIT_FAIL;
	}

    // Generate terrain: ring of mountains then hills along the world border, plains elsewhere.
    // TODO(Marc): Replace with map generation from data transmitted by the server before match start (e.g. image files).
    {
        const ui16 MOUNTAIN_BORDER_DISTANCE = 10;
        const ui16 HILLS_BORDER_DISTANCE = 15;
        const i32 LAKE_HALF_SIZE = 40; // Central lake spans tiles [center - 5, center + 4] on both axes (10x10).
        const i32 LAKE_COAST_THICKNESS = 2;

        world_terrain& terrain = newWorld->terrain;
        for (ui16 y = 0; y < terrain.size_tiles.y; y++)
        {
            for (ui16 x = 0; x < terrain.size_tiles.x; x++)
            {
                // Distance to the nearest world border.
                ui16 d = x;
                if (y < d) d = y;
                if (terrain.size_tiles.x - 1 - x < d) d = terrain.size_tiles.x - 1 - x;
                if (terrain.size_tiles.y - 1 - y < d) d = terrain.size_tiles.y - 1 - y;

                TERRAIN_TILE_TYPE type = TERRAIN_TILE_TYPE::LAND_PLAINS;
                if (d < MOUNTAIN_BORDER_DISTANCE) type = TERRAIN_TILE_TYPE::LAND_MOUNTAINS;
                else if (d < HILLS_BORDER_DISTANCE) type = TERRAIN_TILE_TYPE::LAND_HILLS;
                else
                {
                    // Distance from the lake's outer edge, inward (negative = outside the lake).
                    i32 lakeMinX = terrain.size_tiles.x / 2 - LAKE_HALF_SIZE;
                    i32 lakeMinY = terrain.size_tiles.y / 2 - LAKE_HALF_SIZE;
                    i32 lakeDepth = x - lakeMinX;
                    if (lakeMinX + 2 * LAKE_HALF_SIZE - 1 - x < lakeDepth) lakeDepth = lakeMinX + 2 * LAKE_HALF_SIZE - 1 - x;
                    if ((i32)y - lakeMinY < lakeDepth) lakeDepth = (i32)y - lakeMinY;
                    if (lakeMinY + 2 * LAKE_HALF_SIZE - 1 - (i32)y < lakeDepth) lakeDepth = lakeMinY + 2 * LAKE_HALF_SIZE - 1 - (i32)y;

                    if (lakeDepth >= LAKE_COAST_THICKNESS) type = TERRAIN_TILE_TYPE::WATER_SEA;
                    else if (lakeDepth >= 0) type = TERRAIN_TILE_TYPE::WATER_COAST;
                }

                // Regions are stored row-major, and tiles are row-major within a region.
                ui32 regionIndex = (y / world_terrain::REGION_SIZE) * terrain.size_regions.x + (x / world_terrain::REGION_SIZE);
                ui32 tileIndex = (y % world_terrain::REGION_SIZE) * world_terrain::REGION_SIZE + (x % world_terrain::REGION_SIZE);
                terrain.regions[regionIndex].terrain_types[tileIndex] = type;

                // Also initialize tile ownership to UNOWNED and border count to 4.
                terrain.regions[regionIndex].owners[tileIndex] = world_terrain::UNOWNED_TILE;
                terrain.regions[regionIndex].status[tileIndex].border_count = 4;
            }
        }
    }

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

    world_system_economy_influence_spread(world);
    world_system_economy_contestation(world);
    world_system_economy_pop_growth(world);

    world_system_settlement_spawn_caravans(world);

    world_system_entity_movement_decision(world);
    world_system_entity_movement(world);

    world_system_caravan_arrivals(world);
    world_system_armies_target_reached(world);

    // Increment world age.
    world.age++;
}
