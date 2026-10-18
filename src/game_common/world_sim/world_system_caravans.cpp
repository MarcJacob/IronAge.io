// Caravan systems: spawning caravans between settlements, and their arrivals.

#include "core/math/vec2.h"
#include "game_common/match/world.h"
#include "world_sim.h"

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
                    .destination = settlements.locations[destRandIndex],
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
