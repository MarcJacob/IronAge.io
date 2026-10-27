// Territory systems: settlement influence over world tiles. Claiming / unclaiming tiles, distance to the settlement, influence spread.

#include "game_common/match/world/world_state.h"
#include "game_common/match/world/world_terrain.h"

#include "../world_sim.h"
#include "../world_movement.h"

#include "../entities/world_sim_entity_settlement.h"

struct tile_arable_land_def
{
    ui8 peasant_job_count; // Number of full-efficiency peasant jobs available on that tile.
    ui8 surplus_percent; // How much each peasant produces in surplus on that tile assuming no distance penalties, as a percentage.
};

// Number of peasants worth of arable land per terrain type, and the surplus percentage per peasant at 0 distance.
// Each tile can support a certain number of inhabitants natively, and they feed themselves first. Surplus is what is available to be put on the local market.
// When surplus reaches 0 because of distance penalties, the population on the tile is effectively neutral to the settlement's economy and produces no taxable value.
static constexpr tile_arable_land_def TILE_TERRAIN_ARABLE_LAND[] = {
    { 10, 15 }, // LAND_PLAINS
    { 8, 15 }, // LAND_HILLS
    { 6, 25 }, // LAND_MOUNTAINS

    { 6, 75 }, // WATER_COAST
    { 4, 30 }, // WATER_SEA
};

// Distance below max at which surplus is reduced by 10%, stacking additively (IE if surplus is 100, then ten times this value below max distance will wipe surplus).
static constexpr ui8 LAND_SURPLUS_DECREASE_STEP_TRAVEL_DISTANCE = 30;

// Distance value of a tile with no known route to its settlement.
static constexpr ui16 TERRITORY_NO_DISTANCE = ~0;

// Max distance (Chamfer units) a tile can have to be claimed, taken over or kept by a settlement.
static constexpr ui16 TERRITORY_MAX_DISTANCE = 400;

// A tile is taken over by a neighbouring settlement when its distance to it is shorter by more than this: one orthogonal step over plains.
static constexpr ui16 TERRITORY_TAKEOVER_MARGIN = TRAVEL_DIST_WEIGHT_ORTHOGONAL * TILE_TERRAIN_TRAVEL_COST[(ui8)TERRAIN_TILE_TYPE::LAND_PLAINS];
static constexpr ui16 TERRITORY_TAKEOVER_SETTLEMENTS_PER_TICK = 4; // Max settlements with tiles to check processed per tick.
static constexpr ui16 TERRITORY_TAKEOVER_DECISIONS_MAX = 128; // Max takeovers decided per tick.

// Sea tiles can't be claimed.
static bool world_tile_is_claimable(const match_world_state& world, ui16 region_index, ui16 tile_index)
{
    return world.terrain->regions[region_index].terrain_types[tile_index] != TERRAIN_TILE_TYPE::WATER_SEA;
}

// Returns the surplus output of a tile based on its travel distance from the settlement center.
static ui32 world_get_tile_land_production_surplus(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 travel_distance)
{
    const tile_arable_land_def& arable_land_props = TILE_TERRAIN_ARABLE_LAND[(ui8)world.terrain->regions[region_index].terrain_types[tile_index]];

    ui8 output = arable_land_props.peasant_job_count * arable_land_props.surplus_percent / 100;

    ui16 decreaseStepCount = travel_distance / LAND_SURPLUS_DECREASE_STEP_TRAVEL_DISTANCE;
    if (decreaseStepCount >= 10) return 0;

    ui8 decreasePerStep = output / 10;
    for (ui16 decreaseStep = 0; decreaseStep < decreaseStepCount && output > 0; decreaseStep++)
    {
        output = output - ia_min(output, decreasePerStep);
    }

    return output;
}

// Grows the bounds so they include the given location.
static void world_bounds_include(world_location& bounds_min, world_location& bounds_max, world_location location)
{
    if (location.x < bounds_min.x) bounds_min.x = location.x;
    else if (location.x > bounds_max.x) bounds_max.x = location.x;

    if (location.y < bounds_min.y) bounds_min.y = location.y;
    else if (location.y > bounds_max.y) bounds_max.y = location.y;
}

static void world_influence_update_tile_distance(const match_world_state& world,
        ui16 region_index, ui16 tile_index, ui16 settlement_index, ui16 new_distance)
{
    ui16& currentDistance = world.terrain->regions[region_index].distances[tile_index];

    // Calculate old and new land outputs based on distance.
    ui32 currentOutput = world_get_tile_land_production_surplus(world, region_index, tile_index, currentDistance);
    ui32 newOutput = world_get_tile_land_production_surplus(world, region_index, tile_index, new_distance);

    // Update base land output (subtract old, add new)
    auto& landOutput = world.entities->settlements.base_land_surplus[settlement_index];
    ASSERT(landOutput >= currentOutput);
    landOutput = landOutput - currentOutput + newOutput;

    currentDistance = new_distance;
}

// Get the travel distance a tile would have if owned by the settlement now: lowest distance among all its neighbours (diagonals included) owned by that settlement
// and with a known distance, plus the cost of stepping from it (tile travel cost times the orthogonal or diagonal weight).
// Returns TERRITORY_NO_DISTANCE if there is no such neighbour.
static ui16 world_influence_candidate_distance(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    ui16 travelCost = world_tile_move_cost(*world.terrain, region_index, tile_index, {}); // Determine travel cost based on pathing move cost with no default parameters.
    world_location tileWorldLocation = world_get_tile_location(*world.terrain, region_index, tile_index);

    ui32 bestDist = TERRITORY_NO_DISTANCE;
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(*world.terrain, tileWorldLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;
        if (world.terrain->regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

        ui16 neighbourDist = world.terrain->regions[neighbourRegionIndex].distances[neighbourTileIndex];
        if (neighbourDist == TERRITORY_NO_DISTANCE) continue;

        bool isDiagonal = offset.x != 0 && offset.y != 0;
        ui32 stepDist = neighbourDist + travelCost * (isDiagonal ? TRAVEL_DIST_WEIGHT_DIAGONAL : TRAVEL_DIST_WEIGHT_ORTHOGONAL);
        if (stepDist < bestDist)
        {
            bestDist = stepDist;
        }
    }

    return bestDist >= TERRITORY_NO_DISTANCE ? TERRITORY_NO_DISTANCE : (ui16)bestDist;
}

// Has a tile flag the neighbours owned by the settlement with a distance higher than the given one for a distance recalculation (they may have a shorter route now).
static void world_influence_flag_distance_higher_neighbours(const match_world_state& world,
        ui16 region_index, ui16 tile_index, ui16 settlement_index, ui16 distance)
{
    world_location tileLocation = world_get_tile_location(*world.terrain, region_index, tile_index);
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(*world.terrain, tileLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;
        if (world.terrain->regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

        if (world.terrain->regions[neighbourRegionIndex].distances[neighbourTileIndex] > distance)
        {
            world.terrain->regions[neighbourRegionIndex].status[neighbourTileIndex].distance_recalc = true;
        }
    }
}

// Flags a tile for a takeover check, and its owner as having tiles to check. Unowned tiles are skipped.
static void world_influence_flag_takeover(const match_world_state& world, ui16 region_index, ui16 tile_index)
{
    ui16 owner = world.terrain->regions[region_index].owners[tile_index];
    if (owner == world_terrain::UNOWNED_TILE) return;

    world.terrain->regions[region_index].status[tile_index].takeover_check = true;
    world.entities->settlements.territories[owner].takeover_pending = true;
}

// Flags a tile and its neighbours for a takeover check.
static void world_influence_flag_takeover_around(const match_world_state& world, ui16 region_index, ui16 tile_index)
{
    world_influence_flag_takeover(world, region_index, tile_index);

    world_location tileLocation = world_get_tile_location(*world.terrain, region_index, tile_index);
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(*world.terrain, tileLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;

        world_influence_flag_takeover(world, neighbourRegionIndex, neighbourTileIndex);
    }
}

static void world_influence_unclaim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    ASSERT(region_index < world.terrain->size_regions.x * world.terrain->size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    ui16& distance = world.terrain->regions[region_index].distances[tile_index];

    // Calculate land output based on current distance and subtract from settlement's total
    ui32 tileOutput = world_get_tile_land_production_surplus(world, region_index, tile_index, distance);

    // Decreased land output.
    auto& landOutput = world.entities->settlements.base_land_surplus[settlement_index];
    ASSERT(landOutput >= tileOutput);
    landOutput -= tileOutput;

    // Decrease land size.
    auto& landSize = world.entities->settlements.territories[settlement_index].tile_count;
    ASSERT(landSize > 0);
    landSize--;

    // Decrease peasant capacity. Move peasants over the limit to unemployed population.
    auto& peasantCap = world.entities->settlements.pop_peasants_cap[settlement_index];
    const tile_arable_land_def& arableLandProps = TILE_TERRAIN_ARABLE_LAND[(ui8)world.terrain->regions[region_index].terrain_types[tile_index]];
    peasantCap -= arableLandProps.peasant_job_count;

    auto& peasants = world.entities->settlements.pop_peasants[settlement_index];
    auto& unemployed = world.entities->settlements.pop_unemployed[settlement_index];
    if (peasants > peasantCap)
    {
        unemployed += peasants - peasantCap;
        peasants = peasantCap;
    }

    // Tiles that depended on this one for their shortest path need a full rebuild of the settlement's distances.
    world.entities->settlements.territories[settlement_index].distances_stale = true;

    // Increase all non-diagonal neighbour tile border count owned by the same settlement.
    world_location tileWorldLocation = world_get_tile_location(*world.terrain, region_index, tile_index);
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(*world.terrain, tileWorldLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;
        if (world.terrain->regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

        world.terrain->regions[neighbourRegionIndex].status[neighbourTileIndex].border_count++;
        ASSERT(world.terrain->regions[neighbourRegionIndex].status[neighbourTileIndex].border_count <= 4);
    }

    // Set tile as unowned, and reset its border count to its unowned value.
    world.terrain->regions[region_index].owners[tile_index] = world_terrain::UNOWNED_TILE;
    world.terrain->regions[region_index].status[tile_index].border_count = 4;
    // Reset tile distance value.
    distance = 0;

    world_influence_flag_takeover_around(world, region_index, tile_index);
}

// Claims the tile for the settlement (taking it from its current owner if any) and returns the pressure cost (the tile's peasant job count).
ui16 world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    ASSERT(region_index < world.terrain->size_regions.x * world.terrain->size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);
    ASSERT(world_tile_is_claimable(world, region_index, tile_index));

    ui16 previousOwner = world.terrain->regions[region_index].owners[tile_index];
    if (previousOwner != world_terrain::UNOWNED_TILE && previousOwner != settlement_index)
    {
        world_influence_unclaim_tile(world, region_index, tile_index, previousOwner);
    }

    // Must be computed before the tile becomes owned.
    ui16 claimDistance = world_influence_candidate_distance(world, region_index, tile_index, settlement_index);
    if (claimDistance == TERRITORY_NO_DISTANCE) claimDistance = 0; // No owned neighbour: the settlement's own root tile.
    ASSERT(claimDistance <= TERRITORY_MAX_DISTANCE);

    // Calculate land output based on distance.
    ui32 tileOutput = world_get_tile_land_production_surplus(world, region_index, tile_index, claimDistance);

    world.terrain->regions[region_index].owners[tile_index] = settlement_index;

    // Increase land output.
    auto& landOutput = world.entities->settlements.base_land_surplus[settlement_index];
    landOutput += tileOutput;

    // Increase land size.
    auto& landSize = world.entities->settlements.territories[settlement_index].tile_count;
    landSize += 1;

    // Increase peasant capacity.
    auto& peasantCap = world.entities->settlements.pop_peasants_cap[settlement_index];
    const tile_arable_land_def& arableLandProps = TILE_TERRAIN_ARABLE_LAND[(ui8)world.terrain->regions[region_index].terrain_types[tile_index]];
    peasantCap += arableLandProps.peasant_job_count;

    // Set distance value immediately so the newly claimed tile is valid this tick.
    world.terrain->regions[region_index].distances[tile_index] = claimDistance;
    world.terrain->regions[region_index].status[tile_index].distance_recalc = true;

    // A new connection can improve existing same-settlement paths.
    world_influence_flag_distance_higher_neighbours(world, region_index, tile_index, settlement_index, claimDistance);
    world_influence_flag_takeover_around(world, region_index, tile_index);

    // Tile's own border count starts at 4, then decreases for each non-diagonal neighbour that is owned by the same settlement or off the map edge.
    // Each same-owner neighbour's border count also decreases.
    auto& tileStatus = world.terrain->regions[region_index].status[tile_index];
    tileStatus.border_count = 4;

    world_location tileWorldLocation = world_get_tile_location(*world.terrain, region_index, tile_index);
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(*world.terrain, tileWorldLocation, offset, neighbourRegionIndex, neighbourTileIndex))
        {
            // Map edge is not a border.
            tileStatus.border_count--;
            continue;
        }

        if (world.terrain->regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

        tileStatus.border_count--;
        ASSERT(world.terrain->regions[neighbourRegionIndex].status[neighbourTileIndex].border_count > 0);
        world.terrain->regions[neighbourRegionIndex].status[neighbourTileIndex].border_count--;
    }

    // Each tile costs the peasants jobs count unit of pressure to claim
    return arableLandProps.peasant_job_count;
}

// Rebuilds the distances of all tiles of a settlement as shortest paths from its own tile over its own tiles (they may be longer than before).
// Tiles with no route left are unclaimed. One pass shared by all the tiles lost since the last one.
static void world_influence_distances_rebuild(match_world_state& world, ui16 settlement_index)
{
    world_terrain& terrain = *world.terrain;
    world_entity_settlements& settlements = world.entities->settlements;
    const world_location boundsMin = settlements.territories[settlement_index].bounds_min;
    const world_location boundsMax = settlements.territories[settlement_index].bounds_max;
    auto& landOutput = settlements.base_land_surplus[settlement_index];

    // Take the output of every tile out of the total and forget its distance, except the settlement's own tile.
    for (world_location::val_type x = boundsMin.x; x <= boundsMax.x; x++)
    {
        for (world_location::val_type y = boundsMin.y; y <= boundsMax.y; y++)
        {
            ui16 regionIndex, tileIndex;
            world_get_tile(terrain, {x, y}, regionIndex, tileIndex);
            if (terrain.regions[regionIndex].owners[tileIndex] != settlement_index) continue;
            if (world_location{x, y} == settlements.locations[settlement_index]) continue;

            ui32 tileOutput = world_get_tile_land_production_surplus(world, regionIndex, tileIndex, terrain.regions[regionIndex].distances[tileIndex]);
            ASSERT(landOutput >= tileOutput);
            landOutput -= tileOutput;
            terrain.regions[regionIndex].distances[tileIndex] = TERRITORY_NO_DISTANCE;
        }
    }

    // Sweep the bounds, alternating direction, lowering distances from owned neighbours until nothing changes (distances only go down, so it ends).
    bool changed = true;
    for (bool forward = true; changed; forward = !forward)
    {
        changed = false;
        for (i32 xStep = 0; xStep <= boundsMax.x - boundsMin.x; xStep++)
        {
            for (i32 yStep = 0; yStep <= boundsMax.y - boundsMin.y; yStep++)
            {
                world_location location = forward
                        ? world_location{ (ui16)(boundsMin.x + xStep), (ui16)(boundsMin.y + yStep) }
                        : world_location{ (ui16)(boundsMax.x - xStep), (ui16)(boundsMax.y - yStep) };

                ui16 regionIndex, tileIndex;
                world_get_tile(terrain, location, regionIndex, tileIndex);
                if (terrain.regions[regionIndex].owners[tileIndex] != settlement_index) continue;
                if (location == settlements.locations[settlement_index]) continue;

                ui16 newDistance = world_influence_candidate_distance(world, regionIndex, tileIndex, settlement_index);
                if (newDistance < terrain.regions[regionIndex].distances[tileIndex])
                {
                    terrain.regions[regionIndex].distances[tileIndex] = newDistance;
                    changed = true;
                }
            }
        }
    }

    // Give the output back to reached tiles, unclaim the unreached and the too distant ones (tiles within range never depend on those, so no further rebuild is needed).
    for (world_location::val_type x = boundsMin.x; x <= boundsMax.x; x++)
    {
        for (world_location::val_type y = boundsMin.y; y <= boundsMax.y; y++)
        {
            ui16 regionIndex, tileIndex;
            world_get_tile(terrain, {x, y}, regionIndex, tileIndex);
            if (terrain.regions[regionIndex].owners[tileIndex] != settlement_index) continue;
            if (world_location{x, y} == settlements.locations[settlement_index]) continue;

            ui16 distance = terrain.regions[regionIndex].distances[tileIndex];
            // Output is given back before unclaiming, which takes it out again.
            landOutput += world_get_tile_land_production_surplus(world, regionIndex, tileIndex, distance);
            if (distance > TERRITORY_MAX_DISTANCE) // Includes TERRITORY_NO_DISTANCE.
            {
                world_influence_unclaim_tile(world, regionIndex, tileIndex, settlement_index);
                continue;
            }

            world_influence_flag_takeover_around(world, regionIndex, tileIndex);
        }
    }

    settlements.territories[settlement_index].distances_stale = false; // Unclaiming above flagged it again.
}

void world_system_settlement_territory_distance_recalc(match_world_state& world)
{
    world_terrain& terrain = *world.terrain;
    world_entity_settlements& settlements = world.entities->settlements;

    // Process each settlement independently. Its influence bounds contain all of
    // its claimed tiles, so there is no need to scan unrelated world terrain.
    for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;
        if (settlements.territories[settlementIndex].distances_stale) world_influence_distances_rebuild(world, settlementIndex);

        const world_location boundsMin = settlements.territories[settlementIndex].bounds_min;
        const world_location boundsMax = settlements.territories[settlementIndex].bounds_max;
        for (world_location::val_type x = boundsMin.x; x <= boundsMax.x; x++)
        {
            for (world_location::val_type y = boundsMin.y; y <= boundsMax.y; y++)
            {
                ui16 regionIndex, tileIndex;
                world_get_tile(terrain, {x, y}, regionIndex, tileIndex);

                if (terrain.regions[regionIndex].owners[tileIndex] != settlementIndex) continue;
                auto& tileStatus = terrain.regions[regionIndex].status[tileIndex];
                if (!tileStatus.distance_recalc) continue;
                tileStatus.distance_recalc = false;

                world_location tileLocation = {x, y};
                if (tileLocation == settlements.locations[settlementIndex])
                {
                    world_influence_update_tile_distance(world, regionIndex, tileIndex, settlementIndex, 0);
                    continue;
                }

                // Distances only go down here (a route got shorter: new tile, road). Longer ones come from the rebuild.
                ui16 newDistance = world_influence_candidate_distance(world, regionIndex, tileIndex, settlementIndex);
                if (newDistance == TERRITORY_NO_DISTANCE || newDistance >= terrain.regions[regionIndex].distances[tileIndex]) continue;

                world_influence_update_tile_distance(world, regionIndex, tileIndex, settlementIndex, newDistance);
                world_influence_flag_distance_higher_neighbours(world, regionIndex, tileIndex, settlementIndex, newDistance);
                world_influence_flag_takeover_around(world, regionIndex, tileIndex);
            }
        }
    }
}


// Returns true if the border tile has at least one non-diagonal neighbour the settlement could expand into.
// That means a claimable unclaimed tile within the max distance or a tile that can be pushed into.
static bool world_influence_can_expand(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    world_location tileLocation = world_get_tile_location(*world.terrain, region_index, tile_index);
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(*world.terrain, tileLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;

        ui16 neighbourOwner = world.terrain->regions[neighbourRegionIndex].owners[neighbourTileIndex];
        if (neighbourOwner == world_terrain::UNOWNED_TILE && world_tile_is_claimable(world, neighbourRegionIndex, neighbourTileIndex)
                && world_influence_candidate_distance(world, neighbourRegionIndex, neighbourTileIndex, settlement_index) <= TERRITORY_MAX_DISTANCE) return true;

        // @TODO(Marc): Push mechanics, allowing territories to be redrawn.
    }

    return false;
}

// Runs full influence logic over a border tile owned by the settlement (assumed to have been checked as so).
// bounds_min and bounds_max are kept up-to-date with new tiles.
static void world_influence_border_logic(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index, i32& pressure,
        world_location& bounds_min, world_location& bounds_max)
{
    ASSERT(region_index < world.terrain->size_regions.x * world.terrain->size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    world_location tileLocation = world_get_tile_location(*world.terrain, region_index, tile_index);

    // Check all 4 neighbours for claiming.
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        if (pressure <= 0) return;

        ui16 claimRegionIndex, claimTileIndex;
        if (!world_get_neighbour_tile(*world.terrain, tileLocation, offset, claimRegionIndex, claimTileIndex)) continue;

        ui16 targetOwner = world.terrain->regions[claimRegionIndex].owners[claimTileIndex];
        bool isUnowned = targetOwner == world_terrain::UNOWNED_TILE;
        if (targetOwner == settlement_index) continue;

        // Colonize empty tile
        if (!isUnowned || !world_tile_is_claimable(world, claimRegionIndex, claimTileIndex)) continue;
        if (world_influence_candidate_distance(world, claimRegionIndex, claimTileIndex, settlement_index) > TERRITORY_MAX_DISTANCE) continue;

        pressure -= world_influence_claim_tile(world, claimRegionIndex, claimTileIndex, settlement_index);
        world_bounds_include(bounds_min, bounds_max, { (ui16)(tileLocation.x + offset.x), (ui16)(tileLocation.y + offset.y) });

        // Check border flag and return from the function immediately if tile isn't a border anymore.
        if (world.terrain->regions[region_index].status[tile_index].border_count == 0) return;
    }
}

// Border tile picked for processing by the influence spread.
struct influence_candidate
{
    ui16 region_index, tile_index;
    ui16 distance;
};
static constexpr ui16 INFLUENCE_CANDIDATE_COUNT = 16; // Max border tiles processed per settlement per tick.

// Inserts into the candidate list, kept sorted by ascending distance, replacing the worst entry if full. Ties keep the earliest inserted.
static void world_influence_candidate_insert(influence_candidate* candidates, ui32& candidate_count, const influence_candidate& candidate)
{
    if (candidate_count == INFLUENCE_CANDIDATE_COUNT && candidate.distance >= candidates[INFLUENCE_CANDIDATE_COUNT - 1].distance) return;

    ui32 slot = candidate_count < INFLUENCE_CANDIDATE_COUNT ? candidate_count : INFLUENCE_CANDIDATE_COUNT - 1;
    while (slot > 0 && candidates[slot - 1].distance > candidate.distance)
    {
        candidates[slot] = candidates[slot - 1];
        slot--;
    }
    candidates[slot] = candidate;
    if (candidate_count < INFLUENCE_CANDIDATE_COUNT) candidate_count++;
}

void world_system_settlement_territory(match_world_state& world)
{
    // @TEMP(Marc): Handle turning unemployed pop into peasants here.
    for (ui16 settlementIndex = 0; settlementIndex < world.entities->settlements.max_count; settlementIndex++)
    {
        if (!world.entities->settlements.guids[settlementIndex].is_valid()) continue;

        ui32& peasants = world.entities->settlements.pop_peasants[settlementIndex];
        ui32& unemployed = world.entities->settlements.pop_unemployed[settlementIndex];
        ui32 peasantsCap = world.entities->settlements.pop_peasants_cap[settlementIndex];
        if (peasants < peasantsCap && unemployed > 0)
        {
            ui32 employCount = ia_min(unemployed, peasantsCap - peasants);
            peasants += employCount;
            unemployed -= employCount;
        }
    }

    // For each settlement, border tiles within its influence bounds claim unowned tiles
    // as far as the pressure (unemployed population) allows. Interior tiles are left alone.

    for (ui16 settlementIndex = 0; settlementIndex < world.entities->settlements.max_count; settlementIndex++)
    {
        if (!world.entities->settlements.guids[settlementIndex].is_valid()) continue;

        world_entity_settlement_view settlementView { world.entities->settlements, settlementIndex };

        world_location newBoundsMin = world.entities->settlements.locations[settlementIndex];
        world_location newBoundsMax = world.entities->settlements.locations[settlementIndex];

        i32 pressure = settlementView.pop_unemployed; // Unemployed population seeks to go find themselves a plot of land to farm on, marry and adopt a dog + 2 kitten.

        // Low pressure doesn't unclaim territory. The bounds are still rescanned.

        // Scan the bounds for border tiles, keeping the INFLUENCE_CANDIDATE_COUNT lowest-distance ones sorted (ascending) so expansion goes nearest-first
        // instead of following scan order. Ties keep the earliest found.
        // Every border tile found must stay within the new bounds, even if it isn't picked as a candidate this tick.
        influence_candidate candidates[INFLUENCE_CANDIDATE_COUNT];
        ui32 candidateCount = 0;

        for (world_location::val_type x = world.entities->settlements.territories[settlementIndex].bounds_min.x;
                x <= world.entities->settlements.territories[settlementIndex].bounds_max.x; x++)
        {
            for (world_location::val_type y = world.entities->settlements.territories[settlementIndex].bounds_min.y;
                    y <= world.entities->settlements.territories[settlementIndex].bounds_max.y; y++)
            {
                ui16 regionIndex, tileIndex;
                world_get_tile(*world.terrain, {x, y}, regionIndex, tileIndex);

                if (world.terrain->regions[regionIndex].owners[tileIndex] != settlementIndex) continue;
                if (world.terrain->regions[regionIndex].status[tileIndex].border_count == 0) continue;
                world_bounds_include(newBoundsMin, newBoundsMax, { x, y });

                // Border tiles that can't expand anywhere are kept in the bounds but must not take up candidate slots.
                if (pressure <= 0 || !world_influence_can_expand(world, regionIndex, tileIndex, settlementIndex)) continue;

                world_influence_candidate_insert(candidates, candidateCount,
                        { regionIndex, tileIndex, world.terrain->regions[regionIndex].distances[tileIndex] });
            }
        }

        for (ui32 candidateIndex = 0; candidateIndex < candidateCount && pressure > 0; candidateIndex++)
        {
            world_influence_border_logic(world, candidates[candidateIndex].region_index, candidates[candidateIndex].tile_index,
                    settlementIndex, pressure, newBoundsMin, newBoundsMax);
        }

        world.entities->settlements.territories[settlementIndex].bounds_min = newBoundsMin;
        world.entities->settlements.territories[settlementIndex].bounds_max = newBoundsMax;
    }

}

// A tile a neighbouring settlement is going to take from its owner.
struct takeover_decision
{
    ui16 region_index, tile_index;
    ui16 taker; // Settlement slot.
};

// Checks the tiles of a settlement flagged for a takeover check (clearing the flags) and appends the takeovers to decisions. Only reads distances and owners.
// Returns false if it stopped for lack of room in decisions (the unchecked tiles stay flagged).
static bool world_influence_takeover_scan(const match_world_state& world, ui16 settlement_index, takeover_decision* decisions, ui32& decision_count)
{
    const world_terrain& terrain = *world.terrain;
    const world_entity_settlements& settlements = world.entities->settlements;
    const world_location boundsMin = settlements.territories[settlement_index].bounds_min;
    const world_location boundsMax = settlements.territories[settlement_index].bounds_max;

    for (world_location::val_type x = boundsMin.x; x <= boundsMax.x; x++)
    {
        for (world_location::val_type y = boundsMin.y; y <= boundsMax.y; y++)
        {
            ui16 regionIndex, tileIndex;
            world_get_tile(terrain, {x, y}, regionIndex, tileIndex);
            if (terrain.regions[regionIndex].owners[tileIndex] != settlement_index) continue;
            if (!terrain.regions[regionIndex].status[tileIndex].takeover_check) continue;
            if (decision_count == TERRITORY_TAKEOVER_DECISIONS_MAX) return false;

            terrain.regions[regionIndex].status[tileIndex].takeover_check = false;
            if (world_location{x, y} == settlements.locations[settlement_index]) continue;

            // Best taker among the owners of the neighbours: shortest distance, then lowest slot.
            ui16 bestTaker = world_terrain::UNOWNED_TILE;
            ui16 bestDistance = TERRITORY_NO_DISTANCE;
            for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
            {
                ui16 neighbourRegionIndex, neighbourTileIndex;
                if (!world_get_neighbour_tile(terrain, {x, y}, offset, neighbourRegionIndex, neighbourTileIndex)) continue;

                ui16 neighbourOwner = terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex];
                if (neighbourOwner == world_terrain::UNOWNED_TILE || neighbourOwner == settlement_index) continue;
                if (settlements.pop_unemployed[neighbourOwner] == 0) continue; // Only settlements with unemployed left can take land.

                ui16 distance = world_influence_candidate_distance(world, regionIndex, tileIndex, neighbourOwner);
                if (distance < bestDistance || (distance == bestDistance && distance != TERRITORY_NO_DISTANCE && neighbourOwner < bestTaker))
                {
                    bestTaker = neighbourOwner;
                    bestDistance = distance;
                }
            }

            if (bestTaker == world_terrain::UNOWNED_TILE || bestDistance > TERRITORY_MAX_DISTANCE) continue;
            if ((ui32)bestDistance + TERRITORY_TAKEOVER_MARGIN >= terrain.regions[regionIndex].distances[tileIndex]) continue;

            decisions[decision_count++] = { regionIndex, tileIndex, bestTaker };
        }
    }

    return true;
}

void world_system_settlement_territory_takeover(match_world_state& world)
{
    world_entity_settlements& settlements = world.entities->settlements;
    if (settlements.max_count == 0) return;

    // Sweep 1: decide from the current state, on at most TERRITORY_TAKEOVER_SETTLEMENTS_PER_TICK settlements with tiles to check.
    // The first one rotates with the world age so every settlement gets its turn.
    takeover_decision decisions[TERRITORY_TAKEOVER_DECISIONS_MAX];
    ui32 decisionCount = 0;
    ui16 processedCount = 0;
    for (ui16 step = 0; step < settlements.max_count && processedCount < TERRITORY_TAKEOVER_SETTLEMENTS_PER_TICK; step++)
    {
        ui16 settlementIndex = (ui16)((world.age + step) % settlements.max_count);
        if (!settlements.guids[settlementIndex].is_valid() || !settlements.territories[settlementIndex].takeover_pending) continue;

        processedCount++;
        settlements.territories[settlementIndex].takeover_pending = false;
        if (!world_influence_takeover_scan(world, settlementIndex, decisions, decisionCount))
        {
            settlements.territories[settlementIndex].takeover_pending = true; // Not done.
            break;
        }
    }

    // Sweep 2: apply, in decision order. A taker may have lost the neighbour its route went through: skipped then.
    for (ui32 decisionIndex = 0; decisionIndex < decisionCount; decisionIndex++)
    {
        const takeover_decision& decision = decisions[decisionIndex];
        if (world_influence_candidate_distance(world, decision.region_index, decision.tile_index, decision.taker) > TERRITORY_MAX_DISTANCE) continue;

        world_influence_claim_tile(world, decision.region_index, decision.tile_index, decision.taker);
        world_bounds_include(settlements.territories[decision.taker].bounds_min, settlements.territories[decision.taker].bounds_max,
                world_get_tile_location(*world.terrain, decision.region_index, decision.tile_index));
    }
}
