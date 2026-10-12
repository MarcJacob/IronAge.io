// Implementation file for economic systems in the game, IE anything related to settlements, caravand and other economic-side entities.

#include "core/math/vec2.h"
#include "game_common/match/world.h"
#include "world_sim.h"

// Proximity / travel difficulty over each terrain type.
static const ui8 TILE_TERRAIN_TRAVEL_COST[] = {
    3, // LAND_PLAINS
    5, // LAND_HILLS
    10, // LAND_MOUNTAINS

    1, // WATER_COAST
    4, // WATER_SEA
};

// Food production for each terrain type.
static const ui8 TILE_TERRAIN_BASE_OUTPUT[] = {
    3, // LAND_PLAINS
    2, // LAND_HILLS
    1, // LAND_MOUNTAINS

    10, // WATER_COAST
    1, // WATER_SEA
};

// Proximity is measured with chamfer weights (integer approximation of euclidean distance, ratio 7 / 5 = 1.4 ~ sqrt(2)).
// Stepping to a tile adds its terrain travel cost times the weight.
static constexpr ui16 PROXIMITY_WEIGHT_ORTHOGONAL = 5;
static constexpr ui16 PROXIMITY_WEIGHT_DIAGONAL = 7;

// Proximity contributes to a tile's influence cost divided by this value, so that distance matters less than the tile's own terrain.
static constexpr ui16 INFLUENCE_COST_PROXIMITY_DIVISOR = 10;

// Get influence cost for claiming / keeping a tile.
// Cost is the tile's terrain travel cost plus a fraction of its own proximity (the value it has, or would get, once claimed).
ui16 world_influence_get_tile_cost(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 proximity)
{
    ui16 travelCost = TILE_TERRAIN_TRAVEL_COST[(ui8)world.terrain.regions[region_index].terrain_types[tile_index]];
    return travelCost + proximity / INFLUENCE_COST_PROXIMITY_DIVISOR;
}

// Get the proximity a tile would have if claimed by the settlement now: lowest proximity among all its neighbours (diagonals included) owned by that settlement,
// plus the cost of stepping from it (tile travel cost times the orthogonal or diagonal weight).
// Returns 0 if no neighbour is owned by the settlement (IE the settlement's own root tile).
static ui16 world_influence_get_claim_proximity(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    ui16 travelCost = TILE_TERRAIN_TRAVEL_COST[(ui8)world.terrain.regions[region_index].terrain_types[tile_index]];
    world_location tileWorldLocation = world_get_tile_location(world.terrain, region_index, tile_index);
    bool found = false;
    ui16 claimProximity = 0;
    for (i8 x = -1; x <= 1; x++)
        for (i8 y = -1; y <= 1; y++)
        {
            if (x == 0 && y == 0) continue;
            vec2<i32> relativeLoc = tileWorldLocation + vec2<i8>{x, y};
            if (relativeLoc.x < 0 || relativeLoc.x >= world.terrain.size_tiles.x
                    || relativeLoc.y < 0 || relativeLoc.y >= world.terrain.size_tiles.y) continue;

            ui16 neighbourRegionIndex, neighbourTileIndex;
            world_get_tile(world.terrain, { x + tileWorldLocation.x, y + tileWorldLocation.y }, neighbourRegionIndex, neighbourTileIndex);

            if (world.terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

            bool isDiagonal = x != 0 && y != 0;
            ui16 stepProximity = world.terrain.regions[neighbourRegionIndex].proximities[neighbourTileIndex]
                    + travelCost * (isDiagonal ? PROXIMITY_WEIGHT_DIAGONAL : PROXIMITY_WEIGHT_ORTHOGONAL);
            if (!found || stepProximity < claimProximity)
            {
                claimProximity = stepProximity;
                found = true;
            }
        }

    return claimProximity;
}

void world_influence_unclaim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{ 
    ASSERT(region_index < world.terrain.size_regions.x * world.terrain.size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    ui16& proximity = world.terrain.regions[region_index].proximities[tile_index];
    ui16 cost = world_influence_get_tile_cost(world, region_index, tile_index, proximity);
    ui16 tileOutput = TILE_TERRAIN_BASE_OUTPUT[(ui8)world.terrain.regions[region_index].terrain_types[tile_index]];
    //
    // Decreased used influence and land output.
    auto& usedInfluence = world.entities.settlements.influences[settlement_index].used;
    ASSERT(usedInfluence >= cost);
    auto& landOutput = world.entities.settlements.land_outputs[settlement_index];
    ASSERT(landOutput >= tileOutput);

    usedInfluence -= cost;
    landOutput -= tileOutput;

    // Increase all non-diagonal neighbour tile border count owned by the same settlement.
    world_location tileWorldLocation = world_get_tile_location(world.terrain, region_index, tile_index);
    for (i8 x = -1; x <= 1; x++) 
        for (i8 y = -1; y <= 1; y++) 
        {
            if (ia_abs(x + y) != 1) continue;
            vec2<i32> relativeLoc = tileWorldLocation + vec2<i8>{x, y};
            if (relativeLoc.x < 0 || relativeLoc.x >= world.terrain.size_tiles.x
                    || relativeLoc.y < 0 || relativeLoc.y >= world.terrain.size_tiles.y) continue;

            ui16 claimRegionIndex, claimTileIndex;
            world_get_tile(world.terrain, { x + tileWorldLocation.x, y + tileWorldLocation.y }, claimRegionIndex, claimTileIndex);

            if (world.terrain.regions[claimRegionIndex].owners[claimTileIndex] != settlement_index) continue;

            world.terrain.regions[claimRegionIndex].status[claimTileIndex].border_count++;
            ASSERT(world.terrain.regions[claimRegionIndex].status[claimTileIndex].border_count <= 4);
        }

    // Set tile as unowned, and reset its border count to its unowned value.
    world.terrain.regions[region_index].owners[tile_index] = world_terrain::UNOWNED_TILE;
    world.terrain.regions[region_index].status[tile_index].border_count = 4;
    // Reset tile proximity value.
    proximity = 0;
}

void world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    ASSERT(region_index < world.terrain.size_regions.x * world.terrain.size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    // Must be computed before the tile becomes owned.
    ui16 claimProximity = world_influence_get_claim_proximity(world, region_index, tile_index, settlement_index);
    ui16 cost = world_influence_get_tile_cost(world, region_index, tile_index, claimProximity);
    ui16 tileOutput = TILE_TERRAIN_BASE_OUTPUT[(ui8)world.terrain.regions[region_index].terrain_types[tile_index]];

    world.terrain.regions[region_index].owners[tile_index] = settlement_index;

    // Increase used influence and land output.
    auto& usedInfluence = world.entities.settlements.influences[settlement_index].used;
    auto& landOutput = world.entities.settlements.land_outputs[settlement_index];

    usedInfluence += cost;
    landOutput += tileOutput;

    // Set proximity value.
    world.terrain.regions[region_index].proximities[tile_index] = claimProximity;

    // Tile's own border count starts at 4, then decreases for each non-diagonal neighbour that is owned by the same settlement or off the map edge.
    // Each same-owner neighbour's border count also decreases.
    auto& tileStatus = world.terrain.regions[region_index].status[tile_index];
    tileStatus.border_count = 4;

    world_location tileWorldLocation = world_get_tile_location(world.terrain, region_index, tile_index);
    for (i8 x = -1; x <= 1; x++)
        for (i8 y = -1; y <= 1; y++)
        {
            if (ia_abs(x + y) != 1) continue;
            vec2<i32> relativeLoc = tileWorldLocation + vec2<i8>{x, y};
            if (relativeLoc.x < 0 || relativeLoc.x >= world.terrain.size_tiles.x
                    || relativeLoc.y < 0 || relativeLoc.y >= world.terrain.size_tiles.y)
            {
                // Map edge is not a border.
                tileStatus.border_count--;
                continue;
            }

            ui16 claimRegionIndex, claimTileIndex;
            world_get_tile(world.terrain, { x + tileWorldLocation.x, y + tileWorldLocation.y }, claimRegionIndex, claimTileIndex);

            if (world.terrain.regions[claimRegionIndex].owners[claimTileIndex] != settlement_index) continue;

            tileStatus.border_count--;
            ASSERT(world.terrain.regions[claimRegionIndex].status[claimTileIndex].border_count > 0);
            world.terrain.regions[claimRegionIndex].status[claimTileIndex].border_count--;
        }
}

// Runs full influence logic over a border tile owned by the settlement (assumed to have been checked as so).
// bounds_min and bounds_max are kept up-to-date with new tiles.
void world_influence_border_logic(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index, i32& influence_pressure,
        world_location& bounds_min, world_location& bounds_max)
{
    ASSERT(region_index < world.terrain.size_regions.x * world.terrain.size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    world_location tileLocation = world_get_tile_location(world.terrain, region_index, tile_index);

    // The border tile itself must stay within the new bounds.
    if (tileLocation.x < bounds_min.x) bounds_min.x = tileLocation.x;
    else if (tileLocation.x > bounds_max.x) bounds_max.x = tileLocation.x;

    if (tileLocation.y < bounds_min.y) bounds_min.y = tileLocation.y;
    else if (tileLocation.y > bounds_max.y) bounds_max.y = tileLocation.y;

    // Check all 4 neighbours for claiming.
    for (i8 claimX = -1; claimX <= 1 && influence_pressure > 0; claimX++) 
        for (i8 claimY = -1; claimY <= 1 && influence_pressure > 0; claimY++) 
        {
            if (ia_abs(claimX + claimY) != 1) continue;
            vec2<i32> relativeLoc = tileLocation + vec2<i8>{claimX, claimY};
            if (relativeLoc.x < 0 || relativeLoc.x >= world.terrain.size_tiles.x
                    || relativeLoc.y < 0 || relativeLoc.y >= world.terrain.size_tiles.y) continue;

            ui16 claimRegionIndex, claimTileIndex;
            world_get_tile(world.terrain, { tileLocation.x + claimX, tileLocation.y + claimY }, claimRegionIndex, claimTileIndex);

            if (world.terrain.regions[claimRegionIndex].owners[claimTileIndex] == world_terrain::UNOWNED_TILE)
            {
                // Colonize empty tile if cheap enough.
                ui16 claimProximity = world_influence_get_claim_proximity(world, claimRegionIndex, claimTileIndex, settlement_index);
                ui16 cost = world_influence_get_tile_cost(world, claimRegionIndex, claimTileIndex, claimProximity);
                if (cost <= influence_pressure)
                {
                    world_influence_claim_tile(world, claimRegionIndex, claimTileIndex, settlement_index);
                    influence_pressure -= cost;

                    // Update bounds.
                    if (tileLocation.x + claimX < bounds_min.x) bounds_min.x = tileLocation.x + claimX;
                    else if (tileLocation.x + claimX > bounds_max.x) bounds_max.x = tileLocation.x + claimX;

                    if (tileLocation.y + claimY < bounds_min.y) bounds_min.y = tileLocation.y + claimY;
                    else if (tileLocation.y + claimY > bounds_max.y) bounds_max.y = tileLocation.y + claimY;

                    // Check border flag and return from the function immediately if tile isn't a border anymore.
                    if (world.terrain.regions[region_index].status[tile_index].border_count == 0) return;
                }
            }
            else
            {
                // @TODO(Marc): PUSH BACK MECHANICS
            }
        }
}

// Border tile picked for processing by the influence spread.
struct influence_candidate
{
    ui16 region_index, tile_index;
    ui16 proximity;
};
static constexpr ui16 INFLUENCE_CANDIDATE_COUNT = 16; // Max border tiles processed per settlement per tick.

void world_system_economy_influence_spread(match_world_state& world)
{
    world_terrain& terrain = world.terrain;
    world_entity_settlements& settlements = world.entities.settlements;

    // For each world tile, run a parameterized cellular automata over the settlement's influence bounds.
    // Unclaimed and heartland tiles are left alone.
    // Border tiles check current pressure (differential between Used and Capacity) and can claim or shrink if pressure is great enough to justify it (depending on proximity).

    for (ui16 settlementIndex = 0; settlementIndex < world.entities.settlements.max_count; settlementIndex++)
    {
        if (!world.entities.settlements.guids[settlementIndex].is_valid()) continue;

        world_location newBoundsMin = world.entities.settlements.locations[settlementIndex];
        world_location newBoundsMax = world.entities.settlements.locations[settlementIndex];

        world_entity_settlements::land_influence& settlementInfluence = world.entities.settlements.influences[settlementIndex];
        i32 influencePressure = (i32)settlementInfluence.capacity - (i32)settlementInfluence.used;

        if (influencePressure <= 0) continue; // Low pressure doesn't automatically unclaim territory, but leaves it vulnerable for takeover.

        // Scan the bounds for border tiles, keeping the INFLUENCE_CANDIDATE_COUNT lowest-proximity ones sorted (ascending) so expansion goes nearest-first
        // instead of following scan order. Ties keep the earliest found.
        // Every border tile found must stay within the new bounds, even if it isn't picked as a candidate this tick.
        influence_candidate candidates[INFLUENCE_CANDIDATE_COUNT];
        ui32 candidateCount = 0;

        for (world_location::val_type x = world.entities.settlements.influences[settlementIndex].bounds_min.x;
                x <= world.entities.settlements.influences[settlementIndex].bounds_max.x; x++)
        {
            for (world_location::val_type y = world.entities.settlements.influences[settlementIndex].bounds_min.y;
                    y <= world.entities.settlements.influences[settlementIndex].bounds_max.y; y++)
            {
                ui16 regionIndex, tileIndex;
                world_get_tile(world.terrain, {x, y}, regionIndex, tileIndex);

                if (world.terrain.regions[regionIndex].owners[tileIndex] != settlementIndex) continue;
                if (world.terrain.regions[regionIndex].status[tileIndex].border_count == 0) continue;

                // Update bounds.
                if (x < newBoundsMin.x) newBoundsMin.x = x;
                else if (x > newBoundsMax.x) newBoundsMax.x = x;

                if (y < newBoundsMin.y) newBoundsMin.y = y;
                else if (y > newBoundsMax.y) newBoundsMax.y = y;

                // Insert into the sorted candidate list, replacing the worst entry if full.
                ui16 proximity = world.terrain.regions[regionIndex].proximities[tileIndex];
                if (candidateCount == INFLUENCE_CANDIDATE_COUNT && proximity >= candidates[INFLUENCE_CANDIDATE_COUNT - 1].proximity) continue;

                ui32 slot = candidateCount < INFLUENCE_CANDIDATE_COUNT ? candidateCount : INFLUENCE_CANDIDATE_COUNT - 1;
                while (slot > 0 && candidates[slot - 1].proximity > proximity)
                {
                    candidates[slot] = candidates[slot - 1];
                    slot--;
                }
                candidates[slot] = { regionIndex, tileIndex, proximity };
                if (candidateCount < INFLUENCE_CANDIDATE_COUNT) candidateCount++;
            }
        }

        for (ui32 candidateIndex = 0; candidateIndex < candidateCount && influencePressure > 0; candidateIndex++)
        {
            world_influence_border_logic(world, candidates[candidateIndex].region_index, candidates[candidateIndex].tile_index,
                    settlementIndex, influencePressure, newBoundsMin, newBoundsMax);
        }

        world.entities.settlements.influences[settlementIndex].bounds_min = newBoundsMin;
        world.entities.settlements.influences[settlementIndex].bounds_max = newBoundsMax;
    }
}

void world_system_economy_contestation(match_world_state& world)
{
    world_terrain& terrain = world.terrain;
}

void world_system_economy_pop_growth(match_world_state& world)
{
    world_entity_settlements& settlements = world.entities.settlements;

    // POP GROWTH
    // Population seeks to reach an equilibrium that depends on settlement land / food output and by local wealth.
    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex] == 0) continue;

        // @TEMP(Marc): Crude spread / optimization of work: Run on each settlement every 10 ticks based on their Extra guid field and world age.
        if ((settlements.guids[settlementIndex].get_extra() + world.age) % 10 != 0) continue;

        ui32 targetPopulation = settlements.land_outputs[settlementIndex] / 3;

        ui32& population = settlements.populations[settlementIndex];
        
        i32 growth = ((i64)targetPopulation - (i64)population) * 5 / 100; // Base growth is 5% of difference between current and target.
        if (growth > 0 && (ui32)growth >= 1 + population / 5)
        {
            growth = 1 + (i64)population / 5; // Population cannot grow by more than 5% per growth tick.
        }

        if (population + growth < 1)
        {
            population = 1;
            continue;
        }

        population += growth;

        // Calculate new influence capacity.
        settlements.influences[settlementIndex].capacity = population * 100 + 1000;
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
