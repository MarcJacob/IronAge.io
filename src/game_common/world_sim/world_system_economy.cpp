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

// Contestation parameters. Contestation is a per tile value from 0 to CONTESTATION_MAX.
static constexpr ui8 CONTESTATION_MAX = 255;
static constexpr ui8 CONTESTATION_GAIN = 24; // Per tick, per foreign neighbour tile (diagonals included).
static constexpr ui8 CONTESTATION_DECAY = 2; // Per tick.
static constexpr ui8 CONTESTATION_FALLOFF = 32; // Lost per tile when spreading to neighbouring tiles of the same settlement.
static constexpr i32 CONTESTATION_PUSH_MARGIN = 100; // Per mille of capacity that effective pressure must lead by to push a barely contested tile. Scales down to 0 at full contestation.
static constexpr ui8 CONTESTATION_MAX_FOREIGN_NEIGHBOURS = 7; // Number of foreign neighbours that gives full contestation. All 8 is a special case for later.

// Proximity is measured with chamfer weights (integer approximation of euclidean distance, ratio 7 / 5 = 1.4 ~ sqrt(2)).
// Stepping to a tile adds its terrain travel cost times the weight.
static constexpr ui16 PROXIMITY_WEIGHT_ORTHOGONAL = 5;
static constexpr ui16 PROXIMITY_WEIGHT_DIAGONAL = 7;

// Proximity contributes to a tile's influence cost divided by this value, so that distance matters less than the tile's own terrain.
static constexpr ui16 INFLUENCE_COST_PROXIMITY_DIVISOR = 5;

// Neighbour offsets, non-diagonal only or all 8.
static const vec2<i8> NEIGHBOUR_OFFSETS_ORTHOGONAL[] = { {-1, 0}, {0, -1}, {0, 1}, {1, 0} };
static const vec2<i8> NEIGHBOUR_OFFSETS_ALL[] = { {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1} };

// Gets the region and tile indices of the tile at the given offset from a world location. Returns false if that tile is off the map.
static bool world_get_neighbour_tile(const world_terrain& terrain, world_location location, vec2<i8> offset, ui16& out_region_index, ui16& out_tile_index)
{
    i32 x = location.x + offset.x;
    i32 y = location.y + offset.y;
    if (x < 0 || x >= terrain.size_tiles.x || y < 0 || y >= terrain.size_tiles.y) return false;

    world_get_tile(terrain, { (ui16)x, (ui16)y }, out_region_index, out_tile_index);
    return true;
}

// Grows the bounds so they include the given location.
static void world_bounds_include(world_location& bounds_min, world_location& bounds_max, world_location location)
{
    if (location.x < bounds_min.x) bounds_min.x = location.x;
    else if (location.x > bounds_max.x) bounds_max.x = location.x;

    if (location.y < bounds_min.y) bounds_min.y = location.y;
    else if (location.y > bounds_max.y) bounds_max.y = location.y;
}

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
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(world.terrain, tileWorldLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;
        if (world.terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

        bool isDiagonal = offset.x != 0 && offset.y != 0;
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
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(world.terrain, tileWorldLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;
        if (world.terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

        world.terrain.regions[neighbourRegionIndex].status[neighbourTileIndex].border_count++;
        ASSERT(world.terrain.regions[neighbourRegionIndex].status[neighbourTileIndex].border_count <= 4);
    }

    // Set tile as unowned, and reset its border count to its unowned value.
    world.terrain.regions[region_index].owners[tile_index] = world_terrain::UNOWNED_TILE;
    world.terrain.regions[region_index].status[tile_index].border_count = 4;
    // Reset tile proximity value.
    proximity = 0;
}

// Claims the tile for the settlement and returns the influence cost it was claimed at.
ui16 world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
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
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(world.terrain, tileWorldLocation, offset, neighbourRegionIndex, neighbourTileIndex))
        {
            // Map edge is not a border.
            tileStatus.border_count--;
            continue;
        }

        if (world.terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

        tileStatus.border_count--;
        ASSERT(world.terrain.regions[neighbourRegionIndex].status[neighbourTileIndex].border_count > 0);
        world.terrain.regions[neighbourRegionIndex].status[neighbourTileIndex].border_count--;
    }

    return cost;
}

// Pressure as per mille of capacity. Negative if more influence is used than available. A settlement with no capacity has no pressure.
static i32 world_influence_get_pressure_permille(ui32 capacity, i64 pressure)
{
    return capacity == 0 ? 0 : (i32)(pressure * 1000 / (i64)capacity);
}

// Returns true if the settlement can push into the given tile, owned by another settlement.
// The tile must be contested (which also keeps settlement tiles safe, since they never gain contestation), and the settlement's effective pressure must beat the
// defender's by a margin that shrinks with contestation (CONTESTATION_PUSH_MARGIN at the lowest contestation, none at full contestation).
// Pressures are compared as a fraction of each settlement's capacity (in per mille), so push power doesn't depend on settlement size.
// A positive attacking pressure is multiplied by the ratio of the target tile's proximity (to its own settlement) over the attacking border tile's proximity
// (to the attacking settlement): ten times closer to its core = ten times the pressure.
static bool world_influence_can_push(const match_world_state& world, ui16 border_region_index, ui16 border_tile_index,
        ui16 target_region_index, ui16 target_tile_index, ui16 settlement_index, i32 influence_pressure)
{
    ui16 defenderIndex = world.terrain.regions[target_region_index].owners[target_tile_index];
    ASSERT(defenderIndex != world_terrain::UNOWNED_TILE && defenderIndex != settlement_index);

    ui8 contestation = world.terrain.regions[target_region_index].contestation_levels[target_tile_index];
    if (contestation == 0) return false;

    const world_entity_settlements::land_influence& defenderInfluence = world.entities.settlements.influences[defenderIndex];
    const world_entity_settlements::land_influence& attackerInfluence = world.entities.settlements.influences[settlement_index];

    i32 defenderPressure = world_influence_get_pressure_permille(defenderInfluence.capacity, (i64)defenderInfluence.capacity - (i64)defenderInfluence.used);
    i32 attackerPressure = world_influence_get_pressure_permille(attackerInfluence.capacity, influence_pressure);

    if (attackerPressure > 0)
    {
        // Proximity ratio in per mille. The settlement's own tile has proximity 0, so treat the attacker proximity as at least 1.
        i64 attackerProximity = world.terrain.regions[border_region_index].proximities[border_tile_index];
        if (attackerProximity < 1) attackerProximity = 1;
        i64 proximityRatio = (i64)world.terrain.regions[target_region_index].proximities[target_tile_index] * 1000 / attackerProximity;
        attackerPressure = (i32)((i64)attackerPressure * proximityRatio / 1000);
    }

    i32 requiredMargin = CONTESTATION_PUSH_MARGIN * (CONTESTATION_MAX - contestation) / CONTESTATION_MAX;
    return attackerPressure - defenderPressure > requiredMargin;
}

// Returns true if the border tile has at least one non-diagonal neighbour the settlement could expand into.
// That means an unclaimed tile or a tile that can be pushed into.
static bool world_influence_can_expand(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index, i32 influence_pressure)
{
    world_location tileLocation = world_get_tile_location(world.terrain, region_index, tile_index);
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(world.terrain, tileLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;

        ui16 neighbourOwner = world.terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex];
        if (neighbourOwner == world_terrain::UNOWNED_TILE) return true;
        if (neighbourOwner != settlement_index
                && world_influence_can_push(world, region_index, tile_index, neighbourRegionIndex, neighbourTileIndex, settlement_index, influence_pressure)) return true;
    }

    return false;
}

// Runs full influence logic over a border tile owned by the settlement (assumed to have been checked as so).
// bounds_min and bounds_max are kept up-to-date with new tiles.
void world_influence_border_logic(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index, i32& influence_pressure,
        world_location& bounds_min, world_location& bounds_max)
{
    ASSERT(region_index < world.terrain.size_regions.x * world.terrain.size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    world_location tileLocation = world_get_tile_location(world.terrain, region_index, tile_index);

    // Check all 4 neighbours for claiming.
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        if (influence_pressure <= 0) return;

        ui16 claimRegionIndex, claimTileIndex;
        if (!world_get_neighbour_tile(world.terrain, tileLocation, offset, claimRegionIndex, claimTileIndex)) continue;

        ui16 targetOwner = world.terrain.regions[claimRegionIndex].owners[claimTileIndex];
        bool isUnowned = targetOwner == world_terrain::UNOWNED_TILE;
        if (targetOwner == settlement_index) continue;

        // Colonize empty tile, or push into a foreign tile that is contested enough. Both cost the same.
        if (!isUnowned && !world_influence_can_push(world, region_index, tile_index, claimRegionIndex, claimTileIndex, settlement_index, influence_pressure)) continue;

        // A pushed tile is first unclaimed by its defender, and has its contestation reset.
        if (!isUnowned) world_influence_unclaim_tile(world, claimRegionIndex, claimTileIndex, targetOwner);
        influence_pressure -= world_influence_claim_tile(world, claimRegionIndex, claimTileIndex, settlement_index);
        if (!isUnowned) world.terrain.regions[claimRegionIndex].contestation_levels[claimTileIndex] = 0;

        world_bounds_include(bounds_min, bounds_max, { (ui16)(tileLocation.x + offset.x), (ui16)(tileLocation.y + offset.y) });

        // Check border flag and return from the function immediately if tile isn't a border anymore.
        if (world.terrain.regions[region_index].status[tile_index].border_count == 0) return;
    }
}

// Border tile picked for processing by the influence spread.
struct influence_candidate
{
    ui16 region_index, tile_index;
    ui16 proximity;
};
static constexpr ui16 INFLUENCE_CANDIDATE_COUNT = 16; // Max border tiles processed per settlement per tick.

// Inserts into the candidate list, kept sorted by ascending proximity, replacing the worst entry if full. Ties keep the earliest inserted.
static void world_influence_candidate_insert(influence_candidate* candidates, ui32& candidate_count, const influence_candidate& candidate)
{
    if (candidate_count == INFLUENCE_CANDIDATE_COUNT && candidate.proximity >= candidates[INFLUENCE_CANDIDATE_COUNT - 1].proximity) return;

    ui32 slot = candidate_count < INFLUENCE_CANDIDATE_COUNT ? candidate_count : INFLUENCE_CANDIDATE_COUNT - 1;
    while (slot > 0 && candidates[slot - 1].proximity > candidate.proximity)
    {
        candidates[slot] = candidates[slot - 1];
        slot--;
    }
    candidates[slot] = candidate;
    if (candidate_count < INFLUENCE_CANDIDATE_COUNT) candidate_count++;
}

void world_system_economy_influence_spread(match_world_state& world)
{
    // For each settlement, border tiles within its influence bounds claim unowned tiles, or push into contested foreign ones, as far as the pressure
    // (differential between Capacity and Used) allows. Interior tiles are left alone.

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

                world_bounds_include(newBoundsMin, newBoundsMax, { x, y });

                // Border tiles that can't expand anywhere are kept in the bounds but must not take up candidate slots.
                if (!world_influence_can_expand(world, regionIndex, tileIndex, settlementIndex, influencePressure)) continue;

                world_influence_candidate_insert(candidates, candidateCount,
                        { regionIndex, tileIndex, world.terrain.regions[regionIndex].proximities[tileIndex] });
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
    world_entity_settlements& settlements = world.entities.settlements;

    for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;

        ui16 settlementRegionIndex, settlementTileIndex;
        world_get_tile(terrain, settlements.locations[settlementIndex], settlementRegionIndex, settlementTileIndex);
        const world_location boundsMin = settlements.influences[settlementIndex].bounds_min;
        const world_location boundsMax = settlements.influences[settlementIndex].bounds_max;

        // Decay, then generate contestation from foreign neighbours and spread it to immediate neighbours, over all tiles of the settlement.
        // A tile's contestation goes up to a cap proportional to its foreign neighbour count, and is never lowered by that cap.
        for (i32 x = boundsMin.x; x <= boundsMax.x; x++)
            for (i32 y = boundsMin.y; y <= boundsMax.y; y++)
            {
                ui16 regionIndex, tileIndex;
                world_get_tile(terrain, { (ui16)x, (ui16)y }, regionIndex, tileIndex);

                if (terrain.regions[regionIndex].owners[tileIndex] != settlementIndex) continue;

                ui8& contestation = terrain.regions[regionIndex].contestation_levels[tileIndex];
                contestation -= contestation < CONTESTATION_DECAY ? contestation : CONTESTATION_DECAY;

                // Settlement tiles are invulnerable for now.
                if (regionIndex == settlementRegionIndex && tileIndex == settlementTileIndex) continue;

                // Interior tiles (no non-diagonal neighbour that isn't ours) have no foreign neighbours worth counting.
                if (terrain.regions[regionIndex].status[tileIndex].border_count == 0) continue;

                // Count foreign neighbours, remembering the neighbouring tiles for the spread below.
                ui16 neighbourRegionIndices[8], neighbourTileIndices[8];
                ui32 neighbourCount = 0;
                ui32 foreignCount = 0;
                for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
                {
                    ui16 neighbourRegionIndex, neighbourTileIndex;
                    if (!world_get_neighbour_tile(terrain, { (ui16)x, (ui16)y }, offset, neighbourRegionIndex, neighbourTileIndex)) continue;

                    neighbourRegionIndices[neighbourCount] = neighbourRegionIndex;
                    neighbourTileIndices[neighbourCount] = neighbourTileIndex;
                    neighbourCount++;

                    ui16 neighbourOwner = terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex];
                    if (neighbourOwner != world_terrain::UNOWNED_TILE && neighbourOwner != settlementIndex) foreignCount++;
                }

                if (foreignCount == 0) continue;
                if (foreignCount > CONTESTATION_MAX_FOREIGN_NEIGHBOURS) foreignCount = CONTESTATION_MAX_FOREIGN_NEIGHBOURS;

                ui32 cap = CONTESTATION_MAX * foreignCount / CONTESTATION_MAX_FOREIGN_NEIGHBOURS;
                if (contestation < cap)
                {
                    ui32 newContestation = contestation + CONTESTATION_GAIN * foreignCount;
                    contestation = newContestation < cap ? newContestation : cap;
                }

                // Border tiles spread their contestation to their immediate neighbours of the same settlement (diagonals included), losing CONTESTATION_FALLOFF.
                // The settlement's own tile never gains contestation.
                if (contestation <= CONTESTATION_FALLOFF) continue;
                ui8 spreadContestation = contestation - CONTESTATION_FALLOFF;
                for (ui32 neighbourIndex = 0; neighbourIndex < neighbourCount; neighbourIndex++)
                {
                    ui16 neighbourRegionIndex = neighbourRegionIndices[neighbourIndex];
                    ui16 neighbourTileIndex = neighbourTileIndices[neighbourIndex];
                    if (terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlementIndex) continue;
                    if (neighbourRegionIndex == settlementRegionIndex && neighbourTileIndex == settlementTileIndex) continue;

                    ui8& neighbourContestation = terrain.regions[neighbourRegionIndex].contestation_levels[neighbourTileIndex];
                    if (spreadContestation > neighbourContestation) neighbourContestation = spreadContestation;
                }
            }
    }
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
        
        i32 growth = ((i64)targetPopulation - (i64)population);
        if (growth > 0 && (ui32)growth >= 1 + population / 100)
        {
            growth = 1 + (i64)population / 100; // Population cannot grow by more than 1% per growth tick.
        }
        else if (growth < 0 && ia_abs(growth) > (i64)population / 20)
        {
            growth = -(i64)population / 20; // Population cannot shrink by more than 5% per growth tick.
        }

        if ((i64)population + growth < 1)
        {
            population = 1;
        }
        else
        {
            population += growth;
        }

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
