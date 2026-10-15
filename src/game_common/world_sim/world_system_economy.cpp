// Implementation file for economic systems in the game, IE anything related to settlements, caravand and other economic-side entities.

#include "core/math/vec2.h"
#include "game_common/match/world.h"
#include "world_sim.h"

// Food production for each terrain type.
// 10 units = one supported pop.
static const ui8 TILE_TERRAIN_BASE_OUTPUT[] = {
    40, // LAND_PLAINS
    28, // LAND_HILLS
    15, // LAND_MOUNTAINS

    100, // WATER_COAST
    20, // WATER_SEA
};

// How much population "Fills" one tile of terrain.
// If population is higher than this, the settlement gets pressure to expand.
static const ui8 POP_CAP_PER_TILE = 2;

// Contestation parameters. Contestation is a per tile value from 0 to CONTESTATION_MAX.
static constexpr ui8 CONTESTATION_MAX = 255;
static constexpr ui8 CONTESTATION_GAIN = 24; // Per tick, per foreign neighbour tile (diagonals included).
static constexpr ui8 CONTESTATION_DECAY = 2; // Per tick.
static constexpr ui8 CONTESTATION_FALLOFF = 32; // Lost per tile when spreading to neighbouring tiles of the same settlement.
static constexpr ui8 CONTESTATION_MAX_FOREIGN_NEIGHBOURS = 7; // Number of foreign neighbours that gives full contestation. All 8 is a special case for later.

// Proximity at which land output is reduced to 90%, stacking.
static constexpr ui16 LAND_OUTPUT_DECREASE_STEP_DIST = 20;
// Proximity at which a border tile cannot expand into more tiles. 
static constexpr ui16 MAX_EXPANSION_PROXIMITY = LAND_OUTPUT_DECREASE_STEP_DIST * 10;

// Returns the land output of a tile based on its terrain base output and proximity to settlement root.
static ui32 world_get_tile_land_output(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 proximity)
{
    ui8 output = TILE_TERRAIN_BASE_OUTPUT[(ui8)world.terrain.regions[region_index].terrain_types[tile_index]];

    ui8 decreaseStepCount = proximity / LAND_OUTPUT_DECREASE_STEP_DIST;
    if (decreaseStepCount >= 10) return 0;

    for (ui8 decreaseStep = 0; decreaseStep < decreaseStepCount; decreaseStep++)
    {
        output = output - (output / 10);
    }

    return output;
}

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

static void world_influence_update_tile_proximity(const match_world_state& world,
        ui16 region_index, ui16 tile_index, ui16 settlement_index, ui16 new_proximity)
{
    ui16& currentProximity = world.terrain.regions[region_index].proximities[tile_index];
    
    // Calculate old and new land outputs based on proximity
    ui32 currentOutput = world_get_tile_land_output(world, region_index, tile_index, currentProximity);
    ui32 newOutput = world_get_tile_land_output(world, region_index, tile_index, new_proximity);
    
    // Update land_outputs (subtract old, add new)
    auto& landOutput = world.entities.settlements.land_outputs[settlement_index];
    ASSERT(landOutput >= currentOutput);
    landOutput = landOutput - currentOutput + newOutput;

    currentProximity = new_proximity;
}

// Get the proximity a tile would have if claimed by the settlement now: lowest proximity among all its neighbours (diagonals included) owned by that settlement,
// plus the cost of stepping from it (tile travel cost times the orthogonal or diagonal weight).
// Returns 0 if no neighbour is owned by the settlement (IE the settlement's own root tile).
static ui16 world_influence_get_claim_proximity(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    ui16 travelCost = world_tile_move_cost(world.terrain, region_index, tile_index, {}); // Determine travel cost based on pathing move cost with no default parameters.
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

static void world_influence_flag_same_settlement_neighbours(const match_world_state& world,
        ui16 region_index, ui16 tile_index, ui16 settlement_index, ui16 proximity, bool flag_higher, bool flag_lower)
{
    world_location tileLocation = world_get_tile_location(world.terrain, region_index, tile_index);
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(world.terrain, tileLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;
        if (world.terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

        ui16 neighbourProximity = world.terrain.regions[neighbourRegionIndex].proximities[neighbourTileIndex];
        if ((!flag_higher && !flag_lower)
                || (flag_higher && neighbourProximity > proximity)
                || (flag_lower && neighbourProximity < proximity))
        {
            world.terrain.regions[neighbourRegionIndex].status[neighbourTileIndex].proximity_recalc = true;
        }
    }
}

void world_influence_unclaim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{ 
    ASSERT(region_index < world.terrain.size_regions.x * world.terrain.size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    ui16& proximity = world.terrain.regions[region_index].proximities[tile_index];
    
    // Calculate land output based on current proximity and subtract from settlement's total
    ui32 tileOutput = world_get_tile_land_output(world, region_index, tile_index, proximity);
    
    // Decreased land output.
    auto& landOutput = world.entities.settlements.land_outputs[settlement_index];
    ASSERT(landOutput >= tileOutput);
    landOutput -= tileOutput;

    // Decrease land size.
    auto& landSize = world.entities.settlements.land_sizes[settlement_index];
    ASSERT(landSize > 0);
    landSize--;

    // The tile is about to disappear from this settlement's graph. Recalculate all
    // neighbouring tiles that may have depended on it as their shortest path.
    world_influence_flag_same_settlement_neighbours(world, region_index, tile_index, settlement_index,
            proximity, false, false);

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

// Claims the tile for the settlement and returns the pressure cost (1 unit per tile claimed).
ui16 world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    ASSERT(region_index < world.terrain.size_regions.x * world.terrain.size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    // Must be computed before the tile becomes owned.
    ui16 claimProximity = world_influence_get_claim_proximity(world, region_index, tile_index, settlement_index);
    
    // Calculate land output based on proximity
    ui32 tileOutput = world_get_tile_land_output(world, region_index, tile_index, claimProximity);

    world.terrain.regions[region_index].owners[tile_index] = settlement_index;

    // Increase land output.
    auto& landOutput = world.entities.settlements.land_outputs[settlement_index];
    landOutput += tileOutput;

    // Increase land size.
    auto& landSize = world.entities.settlements.land_sizes[settlement_index];
    landSize += 1;

    // Set proximity value immediately so the newly claimed tile is valid this tick.
    world.terrain.regions[region_index].proximities[tile_index] = claimProximity;
    world.terrain.regions[region_index].status[tile_index].proximity_recalc = true;

    // A new connection can improve existing same-settlement paths.
    world_influence_flag_same_settlement_neighbours(world, region_index, tile_index, settlement_index,
            claimProximity, true, false);

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

    // Each tile costs 1 unit of pressure to claim
    return 1;
}

void world_system_economy_proximity_recalc(match_world_state& world)
{
    world_terrain& terrain = world.terrain;
    world_entity_settlements& settlements = world.entities.settlements;

    // Process each settlement independently. Its influence bounds contain all of
    // its claimed tiles, so there is no need to scan unrelated world terrain.
    for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;

        const world_location boundsMin = settlements.influences[settlementIndex].bounds_min;
        const world_location boundsMax = settlements.influences[settlementIndex].bounds_max;
        for (world_location::val_type x = boundsMin.x; x <= boundsMax.x; x++)
        {
            for (world_location::val_type y = boundsMin.y; y <= boundsMax.y; y++)
            {
                ui16 regionIndex, tileIndex;
                world_get_tile(terrain, {x, y}, regionIndex, tileIndex);

                if (terrain.regions[regionIndex].owners[tileIndex] != settlementIndex) continue;
                auto& tileStatus = terrain.regions[regionIndex].status[tileIndex];
                if (!tileStatus.proximity_recalc) continue;
                tileStatus.proximity_recalc = false;

                world_location tileLocation = {x, y};
                if (tileLocation == settlements.locations[settlementIndex])
                {
                    world_influence_update_tile_proximity(world, regionIndex, tileIndex, settlementIndex, 0);
                    continue;
                }

                ui16 lowestNeighbourProximity = 0;
                vec2<i8> lowestNeighbourOffset = { 0, 0 };
                bool foundLowerNeighbour = false;
                ui16 currentProximity = terrain.regions[regionIndex].proximities[tileIndex];

                for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
                {
                    ui16 neighbourRegionIndex, neighbourTileIndex;
                    if (!world_get_neighbour_tile(terrain, tileLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;
                    if (terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlementIndex) continue;

                    ui16 neighbourProximity = terrain.regions[neighbourRegionIndex].proximities[neighbourTileIndex];
                    if (neighbourProximity >= currentProximity) continue;
                    if (!foundLowerNeighbour || neighbourProximity < lowestNeighbourProximity)
                    {
                        foundLowerNeighbour = true;
                        lowestNeighbourProximity = neighbourProximity;
                        lowestNeighbourOffset = offset;
                    }
                }

                if (!foundLowerNeighbour)
                {
                    world_influence_unclaim_tile(world, regionIndex, tileIndex, settlementIndex);
                    continue;
                }

                TERRAIN_TILE_TYPE tileTerrain = terrain.regions[regionIndex].terrain_types[tileIndex];
                ui8 travelCost = world_tile_move_cost(world.terrain, regionIndex, tileIndex, {});
                bool isDiagonal = lowestNeighbourOffset.x != 0 && lowestNeighbourOffset.y != 0;
                ui16 newProximity = lowestNeighbourProximity
                        + travelCost * (isDiagonal ? PROXIMITY_WEIGHT_DIAGONAL : PROXIMITY_WEIGHT_ORTHOGONAL);

                if (newProximity == currentProximity) continue;

                world_influence_update_tile_proximity(world, regionIndex, tileIndex, settlementIndex, newProximity);
                world_influence_flag_same_settlement_neighbours(world, regionIndex, tileIndex, settlementIndex,
                        newProximity, newProximity < currentProximity, newProximity > currentProximity);
            }
        }
    }
}

// Returns true if the settlement can push into the given tile, owned by another settlement.
// Uses new simplified formula: (attacker_pressure / attacker_population) * proximity_ratio > (defender_pressure / defender_population)
// Defense bonus: If defender_pressure <= 0, flip it to positive and add it to attacker pressure.
static bool world_influence_can_push(const match_world_state& world, ui16 border_region_index, ui16 border_tile_index,
        ui16 target_region_index, ui16 target_tile_index, ui16 settlement_index, i32 attacker_pressure)
{
    ui16 defenderIndex = world.terrain.regions[target_region_index].owners[target_tile_index];
    ASSERT(defenderIndex != world_terrain::UNOWNED_TILE && defenderIndex != settlement_index);

    ui32 attackerPop = world.entities.settlements.populations[settlement_index];
    ui32 defenderPop = world.entities.settlements.populations[defenderIndex];
    if (attackerPop == 0 || defenderPop == 0) return false;

    i32 defenderPressure = world.entities.settlements.influences[defenderIndex].pressure;
    i32 effectiveDefenderPressure = defenderPressure + POP_CAP_PER_TILE; // Give defender pressure as if they'd already lost the tile, for stability.
    i32 effectiveAttackerPressure = attacker_pressure;

    // Defense bonus: If defender pressure <= 0, flip it and add to attacker
    if (defenderPressure <= 0)
    {
        effectiveDefenderPressure = -defenderPressure; // negative or zero
        effectiveAttackerPressure = attacker_pressure - defenderPressure; // -(-defender) becomes +
    }

    // Compute pressure ratios (in per mille)
    i32 attackerRatio = (i32)((i64)effectiveAttackerPressure * 1000 / (i64)attackerPop);
    i32 defenderRatio = (i32)((i64)effectiveDefenderPressure * 1000 / (i64)defenderPop);

    // Proximity ratio: (attacker_tile_claim_proximity) / (defender_tile_proximity + 1)
    // Add 1 to avoid division by zero
    i64 attackerProximity = world_influence_get_claim_proximity(world, target_region_index, target_tile_index, settlement_index);
    i64 defenderProximity = world.terrain.regions[target_region_index].proximities[target_tile_index] + 1;

    i64 proximityRatio = defenderProximity * 1000 / attackerProximity;

    // Compare: attacker_ratio * proximity_ratio > defender_ratio
    i64 attackerEffective = (i64)attackerRatio * proximityRatio;
    
    return attackerEffective > (i64)defenderRatio * 1400; // Give slight advantage to defender.
}

// Returns true if the border tile has at least one non-diagonal neighbour the settlement could expand into.
// That means an unclaimed tile or a tile that can be pushed into.
static bool world_influence_can_expand(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index, i32 pressure)
{
    world_location tileLocation = world_get_tile_location(world.terrain, region_index, tile_index);
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(world.terrain, tileLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;

        ui16 neighbourOwner = world.terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex];
        if (neighbourOwner == world_terrain::UNOWNED_TILE) return true;
        if (neighbourOwner != settlement_index
                && world_influence_can_push(world, region_index, tile_index, neighbourRegionIndex, neighbourTileIndex, settlement_index, pressure)) return true;
    }

    return false;
}

// Runs full influence logic over a border tile owned by the settlement (assumed to have been checked as so).
// bounds_min and bounds_max are kept up-to-date with new tiles.
void world_influence_border_logic(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index, i32& pressure,
        world_location& bounds_min, world_location& bounds_max)
{
    ASSERT(region_index < world.terrain.size_regions.x * world.terrain.size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    world_location tileLocation = world_get_tile_location(world.terrain, region_index, tile_index);

    // Check all 4 neighbours for claiming.
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        if (pressure <= 0) return;

        ui16 claimRegionIndex, claimTileIndex;
        if (!world_get_neighbour_tile(world.terrain, tileLocation, offset, claimRegionIndex, claimTileIndex)) continue;

        ui16 targetOwner = world.terrain.regions[claimRegionIndex].owners[claimTileIndex];
        bool isUnowned = targetOwner == world_terrain::UNOWNED_TILE;
        if (targetOwner == settlement_index) continue;

        // Colonize empty tile, or push into a foreign tile that is contested enough. Both cost the same.
        if (!isUnowned && !world_influence_can_push(world, region_index, tile_index, claimRegionIndex, claimTileIndex, settlement_index, pressure)) continue;

        // A pushed tile is first unclaimed by its defender, and has its contestation reset.
        if (!isUnowned) world_influence_unclaim_tile(world, claimRegionIndex, claimTileIndex, targetOwner);
        pressure -= world_influence_claim_tile(world, claimRegionIndex, claimTileIndex, settlement_index);
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
    // For each settlement, border tiles within its influence bounds claim unowned tiles, or push into contested foreign ones,
    // as far as the pressure (population - land_output * 3) allows. Interior tiles are left alone.

    for (ui16 settlementIndex = 0; settlementIndex < world.entities.settlements.max_count; settlementIndex++)
    {
        if (!world.entities.settlements.guids[settlementIndex].is_valid()) continue;

        world_location newBoundsMin = world.entities.settlements.locations[settlementIndex];
        world_location newBoundsMax = world.entities.settlements.locations[settlementIndex];

        world_entity_settlements::land_influence& settlementInfluence = world.entities.settlements.influences[settlementIndex];
        i32 pressure = settlementInfluence.pressure;

        // Low pressure doesn't automatically unclaim territory, but leaves it vulnerable for takeover. The bounds are still rescanned.

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
                if (pressure <= 0 || !world_influence_can_expand(world, regionIndex, tileIndex, settlementIndex, pressure)) continue;

                world_influence_candidate_insert(candidates, candidateCount,
                        { regionIndex, tileIndex, world.terrain.regions[regionIndex].proximities[tileIndex] });
            }
        }

        for (ui32 candidateIndex = 0; candidateIndex < candidateCount && pressure > 0; candidateIndex++)
        {
            world_influence_border_logic(world, candidates[candidateIndex].region_index, candidates[candidateIndex].tile_index,
                    settlementIndex, pressure, newBoundsMin, newBoundsMax);
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

// Updates settlement pressure based on population and land output.
// Pressure = population - land_output * 3. Positive pressure means surplus population wanting to expand.
void world_system_economy_update_pressure(match_world_state& world)
{
    world_entity_settlements& settlements = world.entities.settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;

        ui32 population = settlements.populations[settlementIndex];
        ui32 landPopCap = settlements.land_sizes[settlementIndex] * POP_CAP_PER_TILE;
        
        i32 newPressure = (i32)population - (i32)(landPopCap);
        settlements.influences[settlementIndex].pressure = newPressure;
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

        ui32 targetPopulation = settlements.land_outputs[settlementIndex] / 10; 

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
