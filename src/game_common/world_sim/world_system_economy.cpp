// Implementation file for economic systems in the game, IE anything related to settlements, caravand and other economic-side entities.

#include "game_common/match/world.h"
#include "world_sim.h"

void world_system_settlement_economy(match_world_state& world)
{
    world_entity_settlements& settlements = world.entities.settlements;

    // POP GROWTH
    // Population seeks to reach an equilibrium that depends on settlement land / food output and by local wealth.
    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex] == 0) continue;

        // @TEMP(Marc): Crude spread / optimization of work: Run on each settlement every 10 ticks based on their Extra guid field and world age.
        if ((settlements.guids[settlementIndex].get_extra() + world.age) % 10 != 0) continue;

        ui32 targetPopulation = settlements.land_outputs[settlementIndex]
                            +   settlements.local_wealths[settlementIndex];

        i32 growth = ((i64)targetPopulation - (i64)settlements.populations[settlementIndex]) * 5 / 100;
        if (growth > 0 && (ui32)growth >= 1 + settlements.populations[settlementIndex] / 100)
        {
            growth = 1 + (i64)settlements.populations[settlementIndex] / 100; // Population cannot grow by more than 1% per tick.
        }

        if (settlements.populations[settlementIndex] + growth < 1)
        {
            settlements.populations[settlementIndex] = 1;
            continue;
        }

        settlements.populations[settlementIndex] += growth;
    }

    world_terrain& terrain = world.terrain;

    // INFLUENCE SPREAD
    // @TEMP(Marc): Temp system. Just set influence of all tiles in a square to 1 with the borders of the square being contested.
    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex] == 0) continue;

        // @TEMP(Marc): Crude spread / optimization of work: Run on each settlement every 10 ticks based on their Extra guid field and world age.
        //if ((settlements.guids[settlementIndex].get_extra() + world.age) % 10 != 0) continue;

        world_location settlementLoc = world.entities.settlements.locations[settlementIndex];
        world_location min = settlementLoc - world_location{10, 10};
        world_location max = settlementLoc + world_location{10, 10};

        min.x = ia_clamped<world_location::val_type>(min.x, 0, world.terrain.size_tiles.x);
        min.y = ia_clamped<world_location::val_type>(min.y, 0, world.terrain.size_tiles.y);
        max.x = ia_clamped<world_location::val_type>(max.x, 0, world.terrain.size_tiles.x);
        max.y = ia_clamped<world_location::val_type>(max.y, 0, world.terrain.size_tiles.y);

        for (world_location::val_type x = min.x; x < max.x; x++)
        {
            for (world_location::val_type y = min.y; y < max.y; y++)
            {
                ui16 regionIndex, tileIndex;
                world_get_tile(world.terrain, {x, y}, regionIndex, tileIndex);

                world_terrain::region& region = world.terrain.regions[regionIndex];

                region.influence_levels[tileIndex] = 255;
                region.contestation_levels[tileIndex] =
                    255 * (x == min.x || x == max.x - 1 || y == min.y || y == max.y - 1);
            }
        }
   }
}

void world_system_settlement_spawn_caravans(match_world_state& world)
{
    world_entity_settlements& settlements = world.entities.settlements;

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
                    .move_target = settlements.locations[destRandIndex],
                },
                .origin_settlement = settlements.guids[settlementIndex],
                .dest_settlement = settlements.guids[destRandIndex],
            };
            world_spawn_caravan(world, newCaravan);
        }
    }
}

void world_system_caravan_arrivals(match_world_state& world)
{
    world_entity_caravans& caravans = world.entities.caravans;
    world_entity_settlements& settlements = world.entities.settlements;

	// Once caravans reach close enough to their destination, despawn them and increase origin & destination local wealth.
	for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
	{
		if (!caravans.guids[caravanIndex].is_valid()) continue;

		const world_location& caravanLoc = caravans.locations[caravanIndex];
		const world_location& destLoc = settlements.locations[caravans.dest_settlements[caravanIndex].get_index()];

		if (caravanLoc == destLoc)
		{
            // Caravan has arrived !

			// Both origin and destination town earn 20 + distance in tiles / 10 + destination wealth / 10 + origin wealth / 20, read before either is increased.
			// A destroyed origin counts as 0 distance and 0 wealth, and earns nothing.
			ui16 destIndex = caravans.dest_settlements[caravanIndex].get_index();
			bool originValid = world_entity_is_valid(world, caravans.origin_settlements[caravanIndex]);
			ui16 originIndex = originValid ? caravans.origin_settlements[caravanIndex].get_index() : 0;

			ui32 distanceTiles = originValid ? vec2_dist(settlements.locations[originIndex], destLoc) : 0;
			ui32 originWealth = originValid ? settlements.local_wealths[originIndex] : 0;
			ui32 earnings = 20 + distanceTiles / 10 + settlements.local_wealths[destIndex] / 10 + originWealth / 20;

			settlements.local_wealths[destIndex] += earnings;
			if (originValid) settlements.local_wealths[originIndex] += earnings;

            // Get rid of caravan.
            world_destroy_caravan(world, caravans.guids[caravanIndex]);
		}
	}
}
