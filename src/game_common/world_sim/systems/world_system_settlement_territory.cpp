// Territory systems: settlement influence over world tiles. Claiming / unclaiming tiles, proximity to the settlement, influence spread and contestation.

#include "game_common/match/world/world_state.h"
#include "game_common/match/world/world_terrain.h"

#include "../world_sim.h"
#include "../world_movement.h"

#include "../entities/world_sim_entity_settlement.h"

struct tile_arable_land_def
{
    ui8 peasant_job_count; // Number of full-efficiency peasant jobs available on that tile.
    ui8 surplus_percent; // How much each peasant produces in surplus on that tile assuming no proximity penalties, as a percentage.
};

// Number of peasants worth of arable land per terrain type, and the surplus percentage per peasant at full proximity.
// Each tile can support a certain number of inhabitants natively, and they feed themselves first. Surplus is what is available to be put on the local market.
// When surplus reaches 0 because of proximity penalties, the population on the tile is effectively neutral to the settlement's economy and produce no taxable value.
static constexpr tile_arable_land_def TILE_TERRAIN_ARABLE_LAND[] = {
    { 40, 100 }, // LAND_PLAINS
    { 30, 75 }, // LAND_HILLS
    { 8, 75 }, // LAND_MOUNTAINS

    { 30, 125 }, // WATER_COAST
    { 30, 60 }, // WATER_SEA
};

// Proximity below max at which surplus is reduced by 10%, stacking additively (IE if surplus is 100, then ten times this value below max proximity will wipe surplus).
static constexpr ui8 LAND_SURPLUS_DECREASE_STEP_PROXIMITY = 10;

static constexpr ui16 PROXIMITY_MAX = ~0;

// Returns the surplus output of a tile based on its proxmity to the settlement center.
static ui32 world_get_tile_land_production_surplus(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 proximity)
{
    const tile_arable_land_def& arable_land_props = TILE_TERRAIN_ARABLE_LAND[(ui8)world.terrain->regions[region_index].terrain_types[tile_index]];

    ui8 output = arable_land_props.peasant_job_count * arable_land_props.surplus_percent / 100;

    ui16 decreaseStepCount = (255 - proximity) / LAND_SURPLUS_DECREASE_STEP_PROXIMITY;
    if (decreaseStepCount >= 10) return 0;

    for (ui16 decreaseStep = 0; decreaseStep < decreaseStepCount; decreaseStep++)
    {
        output = output - (output / 5);
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

static void world_influence_update_tile_proximity(const match_world_state& world,
        ui16 region_index, ui16 tile_index, ui16 settlement_index, ui16 new_proximity)
{
    ui16& currentProximity = world.terrain->regions[region_index].proximities[tile_index];

    // Calculate old and new land outputs based on proximity
    ui32 currentOutput = world_get_tile_land_production_surplus(world, region_index, tile_index, currentProximity);
    ui32 newOutput = world_get_tile_land_production_surplus(world, region_index, tile_index, new_proximity);

    // Update base land output (subtract old, add new)
    auto& landOutput = world.entities->settlements.base_land_surplus[settlement_index];
    ASSERT(landOutput >= currentOutput);
    landOutput = landOutput - currentOutput + newOutput;

    currentProximity = new_proximity;
}

// Get the proximity a tile would have if claimed by the settlement now: lowest proximity among all its neighbours (diagonals included) owned by that settlement,
// plus the cost of stepping from it (tile travel cost times the orthogonal or diagonal weight).
// Returns 0 if no neighbour is owned by the settlement (IE the settlement's own root tile).
static ui16 world_influence_get_claim_proximity(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    ui16 travelCost = world_tile_move_cost(*world.terrain, region_index, tile_index, {}); // Determine travel cost based on pathing move cost with no default parameters.
    world_location tileWorldLocation = world_get_tile_location(*world.terrain, region_index, tile_index);

    ui16 claimProximity = 0;
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(*world.terrain, tileWorldLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;
        if (world.terrain->regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

        bool isDiagonal = offset.x != 0 && offset.y != 0;
        ui16 stepProximity = world.terrain->regions[neighbourRegionIndex].proximities[neighbourTileIndex]
                - travelCost * (isDiagonal ? PROXIMITY_WEIGHT_DIAGONAL : PROXIMITY_WEIGHT_ORTHOGONAL);
        if (stepProximity > claimProximity)
        {
            claimProximity = stepProximity;
        }
    }

    return claimProximity;
}

// Has a tile flag all of its neighbours for proximity recalculation.
static void world_influence_flag_proximity_neighbours(const match_world_state& world,
        ui16 region_index, ui16 tile_index, ui16 settlement_index, ui16 proximity, bool flag_higher, bool flag_lower)
{
    world_location tileLocation = world_get_tile_location(*world.terrain, region_index, tile_index);
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(*world.terrain, tileLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;
        if (world.terrain->regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlement_index) continue;

        ui16 neighbourProximity = world.terrain->regions[neighbourRegionIndex].proximities[neighbourTileIndex];
        if ((!flag_higher && !flag_lower)
                || (flag_higher && neighbourProximity > proximity)
                || (flag_lower && neighbourProximity < proximity))
        {
            world.terrain->regions[neighbourRegionIndex].status[neighbourTileIndex].proximity_recalc = true;
        }
    }
}

static void world_influence_unclaim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    ASSERT(region_index < world.terrain->size_regions.x * world.terrain->size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    ui16& proximity = world.terrain->regions[region_index].proximities[tile_index];

    // Calculate land output based on current proximity and subtract from settlement's total
    ui32 tileOutput = world_get_tile_land_production_surplus(world, region_index, tile_index, proximity);

    // Decreased land output.
    auto& landOutput = world.entities->settlements.base_land_surplus[settlement_index];
    ASSERT(landOutput >= tileOutput);
    landOutput -= tileOutput;

    // Decrease land size.
    auto& landSize = world.entities->settlements.territories[settlement_index].tile_count;
    ASSERT(landSize > 0);
    landSize--;

    // The tile is about to disappear from this settlement's graph. Recalculate all
    // neighbouring tiles that may have depended on it as their shortest path.
    world_influence_flag_proximity_neighbours(world, region_index, tile_index, settlement_index,
            proximity, false, false);

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
    // Reset tile proximity value.
    proximity = 0;
}

// Claims the tile for the settlement and returns the pressure cost (1 unit per tile claimed).
ui16 world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index)
{
    ASSERT(region_index < world.terrain->size_regions.x * world.terrain->size_regions.y);
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    // Must be computed before the tile becomes owned.
    ui16 claimProximity = world_influence_get_claim_proximity(world, region_index, tile_index, settlement_index);

    // Calculate land output based on proximity
    ui32 tileOutput = world_get_tile_land_production_surplus(world, region_index, tile_index, claimProximity);

    world.terrain->regions[region_index].owners[tile_index] = settlement_index;

    // Increase land output.
    auto& landOutput = world.entities->settlements.base_land_surplus[settlement_index];
    landOutput += tileOutput;

    // Increase land size.
    auto& landSize = world.entities->settlements.territories[settlement_index].tile_count;
    landSize += 1;

    // Set proximity value immediately so the newly claimed tile is valid this tick.
    world.terrain->regions[region_index].proximities[tile_index] = claimProximity;
    world.terrain->regions[region_index].status[tile_index].proximity_recalc = true;

    // A new connection can improve existing same-settlement paths.
    world_influence_flag_proximity_neighbours(world, region_index, tile_index, settlement_index,
            claimProximity, true, false);

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

    // Each tile costs 1 unit of pressure to claim
    return 1;
}

void world_system_settlement_territory_proximity_recalc(match_world_state& world)
{
    world_terrain& terrain = *world.terrain;
    world_entity_settlements& settlements = world.entities->settlements;

    // Process each settlement independently. Its influence bounds contain all of
    // its claimed tiles, so there is no need to scan unrelated world terrain.
    for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;

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
                if (!tileStatus.proximity_recalc) continue;
                tileStatus.proximity_recalc = false;

                world_location tileLocation = {x, y};
                if (tileLocation == settlements.locations[settlementIndex])
                {
                    world_influence_update_tile_proximity(world, regionIndex, tileIndex, settlementIndex, PROXIMITY_MAX);
                    continue;
                }

                ui16 highestNeighbourProximity = 0;
                vec2<i8> highestProximityNeighbour = { 0, 0 };
                bool foundHigherNeighbour = false;
                ui16 currentProximity = terrain.regions[regionIndex].proximities[tileIndex];

                for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
                {
                    ui16 neighbourRegionIndex, neighbourTileIndex;
                    if (!world_get_neighbour_tile(terrain, tileLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;
                    if (terrain.regions[neighbourRegionIndex].owners[neighbourTileIndex] != settlementIndex) continue;

                    ui16 neighbourProximity = terrain.regions[neighbourRegionIndex].proximities[neighbourTileIndex];
                    if (neighbourProximity <= currentProximity) continue;
                    if (!foundHigherNeighbour || neighbourProximity < highestNeighbourProximity)
                    {
                        foundHigherNeighbour = true;
                        highestNeighbourProximity = neighbourProximity;
                        highestProximityNeighbour = offset;
                    }
                }

                if (!foundHigherNeighbour)
                {
                    world_influence_unclaim_tile(world, regionIndex, tileIndex, settlementIndex);
                    continue;
                }

                TERRAIN_TILE_TYPE tileTerrain = terrain.regions[regionIndex].terrain_types[tileIndex];
                ui8 travelCost = world_tile_move_cost(*world.terrain, regionIndex, tileIndex, {});
                bool isDiagonal = highestProximityNeighbour.x != 0 && highestProximityNeighbour.y != 0;
                ui16 newProximity = highestNeighbourProximity
                        - travelCost * (isDiagonal ? PROXIMITY_WEIGHT_DIAGONAL : PROXIMITY_WEIGHT_ORTHOGONAL);

                if (newProximity == currentProximity) continue;

                world_influence_update_tile_proximity(world, regionIndex, tileIndex, settlementIndex, newProximity);
                world_influence_flag_proximity_neighbours(world, regionIndex, tileIndex, settlementIndex,
                        newProximity, newProximity < currentProximity, newProximity > currentProximity);
            }
        }
    }
}


// Returns true if the border tile has at least one non-diagonal neighbour the settlement could expand into.
// That means an unclaimed tile or a tile that can be pushed into.
static bool world_influence_can_expand(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index, i32 pressure)
{
    world_location tileLocation = world_get_tile_location(*world.terrain, region_index, tile_index);
    for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ORTHOGONAL)
    {
        ui16 neighbourRegionIndex, neighbourTileIndex;
        if (!world_get_neighbour_tile(*world.terrain, tileLocation, offset, neighbourRegionIndex, neighbourTileIndex)) continue;

        ui16 neighbourOwner = world.terrain->regions[neighbourRegionIndex].owners[neighbourTileIndex];
        if (neighbourOwner == world_terrain::UNOWNED_TILE) return true;

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
        if (!isUnowned) continue;

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

void world_system_settlement_territory(match_world_state& world)
{
    // For each settlement, border tiles within its influence bounds claim unowned tiles, or push into contested foreign ones,
    // as far as the pressure (population - land_output * 3) allows. Interior tiles are left alone.

    for (ui16 settlementIndex = 0; settlementIndex < world.entities->settlements.max_count; settlementIndex++)
    {
        if (!world.entities->settlements.guids[settlementIndex].is_valid()) continue;

        world_entity_settlement_view settlementView { world.entities->settlements, settlementIndex };

        world_location newBoundsMin = world.entities->settlements.locations[settlementIndex];
        world_location newBoundsMax = world.entities->settlements.locations[settlementIndex];

        world_entity_settlements::territory& settlementTerritory = world.entities->settlements.territories[settlementIndex];
        i32 pressure = settlementView.pop_peasants - settlementView.pop_peasants_cap + settlementView.pop_unemployed;

        // Low pressure doesn't automatically unclaim territory, but leaves it vulnerable for takeover. The bounds are still rescanned.

        // Scan the bounds for border tiles, keeping the INFLUENCE_CANDIDATE_COUNT lowest-proximity ones sorted (ascending) so expansion goes nearest-first
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
                if (pressure <= 0 || !world_influence_can_expand(world, regionIndex, tileIndex, settlementIndex, pressure)) continue;

                world_influence_candidate_insert(candidates, candidateCount,
                        { regionIndex, tileIndex, world.terrain->regions[regionIndex].proximities[tileIndex] });
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
