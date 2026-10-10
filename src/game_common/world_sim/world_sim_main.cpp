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

constexpr ui32 SETTLEMENTS_PER_REGION = 10;
constexpr ui32 MICRO_ENTITIES_PER_TYPE_PER_REGION = 32;

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
        const i32 LAKE_HALF_SIZE = 5; // Central lake spans tiles [center - 5, center + 4] on both axes (10x10).
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

                TERRAIN_TILE_TYPE type = LAND_PLAINS;
                if (d < MOUNTAIN_BORDER_DISTANCE) type = LAND_MOUNTAINS;
                else if (d < HILLS_BORDER_DISTANCE) type = LAND_HILLS;
                else
                {
                    // Distance from the lake's outer edge, inward (negative = outside the lake).
                    i32 lakeMinX = terrain.size_tiles.x / 2 - LAKE_HALF_SIZE;
                    i32 lakeMinY = terrain.size_tiles.y / 2 - LAKE_HALF_SIZE;
                    i32 lakeDepth = x - lakeMinX;
                    if (lakeMinX + 2 * LAKE_HALF_SIZE - 1 - x < lakeDepth) lakeDepth = lakeMinX + 2 * LAKE_HALF_SIZE - 1 - x;
                    if ((i32)y - lakeMinY < lakeDepth) lakeDepth = (i32)y - lakeMinY;
                    if (lakeMinY + 2 * LAKE_HALF_SIZE - 1 - (i32)y < lakeDepth) lakeDepth = lakeMinY + 2 * LAKE_HALF_SIZE - 1 - (i32)y;

                    if (lakeDepth >= LAKE_COAST_THICKNESS) type = WATER_SEA;
                    else if (lakeDepth >= 0) type = WATER_COAST;
                }

                // Regions are stored row-major, and tiles are row-major within a region.
                ui32 regionIndex = (y / world_terrain::REGION_SIZE) * terrain.size_regions.x + (x / world_terrain::REGION_SIZE);
                ui32 tileIndex = (y % world_terrain::REGION_SIZE) * world_terrain::REGION_SIZE + (x % world_terrain::REGION_SIZE);
                terrain.regions[regionIndex].terrain_types[tileIndex] = type;
            }
        }
    }

    // Apply start params to world state
    for (ui32 settlementIndex = 0; settlementIndex < start_params.start_settlement_count; settlementIndex++)
    {
        world_entity_settlements::single& settlementStartState = start_params.get_settlement_start_state(settlementIndex);
        entity_guid settlementGUID = world_spawn_settlement(*newWorld, settlementStartState);

        // Temp: Spawn an army near every settlement.
        world_location startLoc = world_get_entity_location(*newWorld, settlementGUID) + vec2i{ -10, 10 };

        world_entity_armies::single newArmy = {
            .owner = settlementStartState.owner,
            .location = startLoc,
            .movement = {
                .travel_speed = 50,
                .move_target = startLoc,
            },
            .composition = {
                .levies = 100,
            },
        };

        entity_guid armyGUID = world_spawn_army(*newWorld, newArmy);
    }

	return newWorld;

WORLD_INIT_FAIL:
	memory.allocated_count = memoryStart;
	return nullptr;
}

void world_state_tick(match_world_state& world)
{
	// TEMP WORLD SIMULATION CODE
	// TODO(Marc): Move to a dedicated subcomponent of the game common code. Make it multithreadable.
    //
	// Pseudo-code / Sequence:
	// - 1: Settlement growth / eco tick (log growth based on local wealth / population).
	// - 2: Caravans & Army movement target determination (caravans flee from nearby non-friendly armies or go towards destination, armies go to attack target or move target if set, if not engaged).
	// - 3: Caravans & Army movement (Move towards target at constant speed)
	// - 4: Army engagement / fighting (If army touches its target (ENTITY_WORLD_SIZE), start engagement.
	//		- Engagement system: Create Engagement entity.
	// - 5: Engagement entities: caravans are instantly flagged for destruction, armies take manpower damage and deal manpower damage based on some simple formula. If no manpower left, get destroyed.
	// - 6: Caravans arrival in target settlement (Increase origin settlement local wealth by 1) then get destroyed.
	// - 7: Caravans automatically spawn from settlements if 1000 / local wealth % match tick == 0. Destination is chosen at as much random as possible without libc deterministically.

	world_entity_settlements& settlements = world.entities.settlements;
	world_entity_caravans& caravans = world.entities.caravans;
	world_entity_armies& armies = world.entities.armies;

	// Temp: Settlements have a 1% chance per tick to grow by 5% of their population (at least 1).
	for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex] == 0) continue;

		if (world.rand_gen.next_range(0.f, 1.f) < 0.01f)
		{
			ui32 growth = settlements.populations[settlementIndex] * 5 / 100;
			settlements.populations[settlementIndex] += growth > 0 ? growth : 1;
		}
	}

	// Temp: Randomly spawn caravans at settlements.
	for (ui32 settlementIndex = 0; settlementIndex < world.entities.settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid()) continue;

		// Randomly spawn caravan if there are more settlements on the map.
		if (settlements.active_count > 1 && world.rand_gen.next_range(0.f, 1.f) < 0.001f)
		{
			// Pick a random active settlement other than this one. Slots can be empty now that settlements get destroyed.
			ui16 destRandOrdinal = world.rand_gen.next_range<ui16>(0, settlements.active_count - 1) % (settlements.active_count - 1);
			ui16 destRandIndex = 0;
			for (;; destRandIndex++)
			{
				if (destRandIndex == settlementIndex || !settlements.guids[destRandIndex].is_valid()) continue;
				if (destRandOrdinal == 0) break;
				destRandOrdinal--;
			}

			world_entity_caravans::single newCaravan = {
				.location = settlements.locations[settlementIndex],
				.movement =
				{
					.travel_speed = 40,
					.move_target = settlements.locations[destRandIndex],
				},
				.origin_settlement = settlements.guids[settlementIndex],
				.dest_settlement = settlements.guids[destRandIndex],
			};
			world_spawn_caravan(world, newCaravan);
		}
	}

	// Make sure caravans have their movement target set to their destination settlement.
	for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
	{
		if (!caravans.guids[caravanIndex].is_valid()) continue;

		// Despawn caravans whose destination settlement was destroyed.
		if (!world_entity_is_valid(world, caravans.dest_settlements[caravanIndex]))
		{
			caravans.guids[caravanIndex] = INVALID_ENTITY_GUID;
			caravans.active_count--;
			continue;
		}

		const world_location& destLoc = settlements.locations[caravans.dest_settlements[caravanIndex].get_index()];
		caravans.movements[caravanIndex].move_target = destLoc;
	}

	// Armies committed to an attack chase their target's current location. They stop if the target no longer exists.
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !armies.action_targets[armyIndex].is_valid()) continue;

		if (world_entity_is_valid(world, armies.action_targets[armyIndex]))
		{
			armies.movements[armyIndex].move_target = world_get_entity_location(world, armies.action_targets[armyIndex]);
		}
		else
		{
			armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
			armies.movements[armyIndex].move_target = armies.locations[armyIndex];
		}
	}

	// TEMP: Move armies and caravans towards their destination.
	auto process_entity_movement = [&](world_entity_movement& movement, world_location& location)
		{
			if (movement.move_target == location) return;

			vec2<i32> travelVec = (vec2<i32>)(movement.move_target) - location;

			// Figure out a distribution over the 0, 256 range of both axis representing an absolute travel direction.
			// The relative values of the axis will determine which will get more of the movement.
			vec2<ui8> travelDir;
			if (travelVec.y == 0) travelDir = { 255, 0 };
			else if (travelVec.x == 0) travelDir = { 0, 255 };
			else
			{
				ui16 travelAbsX = ia_abs(travelVec.x), travelAbsY = ia_abs(travelVec.y);
				if (travelAbsX == travelAbsY) travelDir = { 127, 127 };
				// Get a reasonably accurate measure of how much larger one is compared to the other and use it to determine distribution.
				if (travelAbsX > travelAbsY)
				{
					ui32 xMult = travelAbsX * 1000 / travelAbsY;
					ui8 parts = 255 * 1000 / (xMult + 1000);
					travelDir.x = 255 - parts;
					travelDir.y = parts;
				}
				else
				{
					ui32 yMult = travelAbsY * 1000 / travelAbsX;
					ui8 parts = 255 * 1000 / (yMult + 1000);
					travelDir.y = 255 - parts;
					travelDir.x = parts;
				}
			}

			// Turn the travel vector into a signs vector.
			travelVec = { (travelVec.x >= 0) ? 1 : -1, (travelVec.y >= 0) ? 1 : -1 };

			// With absolute travel vector, we can determine where to apply the movement value of the entity.
			i16 xTravel = travelDir.x * 1000 / 255 * movement.travel_speed / 100;
			i16 yTravel = travelDir.y * 1000 / 255 * movement.travel_speed / 100;
			
			i16 newFracX = movement.fractional_loc.x + (i16)xTravel * travelVec.x;
			i16 newFracY = movement.fractional_loc.y + (i16)yTravel * travelVec.y;

			while (newFracX > 100 && location.x < movement.move_target.x) { location.x++; newFracX -= 200; }
			while (newFracX < -100 && location.x > movement.move_target.x) { location.x--; newFracX += 200; }

			while (newFracY > 100 && location.y < movement.move_target.y) { location.y++; newFracY -= 200; }
			while (newFracY < -100 && location.y > movement.move_target.y) { location.y--; newFracY += 200; }

			movement.fractional_loc.x = newFracX * (location.x != movement.move_target.x);
			movement.fractional_loc.y = newFracY * (location.y != movement.move_target.y);
		};

	for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
	{
		if (!caravans.guids[caravanIndex].is_valid()) continue;

		process_entity_movement(caravans.movements[caravanIndex], caravans.locations[caravanIndex]);
	}

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;

		process_entity_movement(armies.movements[armyIndex], armies.locations[armyIndex]);
	}

	// Armies that got close enough to their attack target resolve the attack, which ends the chase.
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !armies.action_targets[armyIndex].is_valid()) continue;

		// An earlier resolution this tick may have destroyed the target.
		entity_guid target = armies.action_targets[armyIndex];
		if (!world_entity_is_valid(world, target)) continue; // Handled by the chase code next tick.

		if (vec2_dist_squared(armies.locations[armyIndex], world_get_entity_location(world, target))
			> ATTACK_REACH_TILES * ATTACK_REACH_TILES) continue;

		armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
		armies.movements[armyIndex].move_target = armies.locations[armyIndex];
		world_resolve_attack(world, armies.guids[armyIndex], target);
	}

	// Once caravans reach close enough to their destination, despawn them and increase origin & destination local wealth.
	for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
	{
		if (!caravans.guids[caravanIndex].is_valid()) continue;

		const world_location& caravanLoc = caravans.locations[caravanIndex];
		const world_location& destLoc = settlements.locations[caravans.dest_settlements[caravanIndex].get_index()];

		if (vec2_dist_squared(caravanLoc, destLoc) < 5)
		{
			// Despawn caravan.
			caravans.guids[caravanIndex] = INVALID_ENTITY_GUID;
			caravans.active_count--;

			// Both origin and destination town earn 20 + distance in tiles + destination wealth / 10 + origin wealth / 20, read before either is increased.
			// A destroyed origin counts as 0 distance and 0 wealth, and earns nothing.
			ui16 destIndex = caravans.dest_settlements[caravanIndex].get_index();
			bool originValid = world_entity_is_valid(world, caravans.origin_settlements[caravanIndex]);
			ui16 originIndex = originValid ? caravans.origin_settlements[caravanIndex].get_index() : 0;

			ui32 distanceTiles = originValid ? vec2_dist(settlements.locations[originIndex], destLoc) : 0;
			ui32 originWealth = originValid ? settlements.local_wealth[originIndex] : 0;
			ui32 earnings = 20 + distanceTiles + settlements.local_wealth[destIndex] / 10 + originWealth / 20;

			settlements.local_wealth[destIndex] += earnings;
			if (originValid) settlements.local_wealth[originIndex] += earnings;
		}
	}

    // Increment world age.
    world.age++;
}
