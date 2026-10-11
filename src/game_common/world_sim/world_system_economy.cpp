// Implementation file for economic systems in the game, IE anything related to settlements, caravand and other economic-side entities.

#include "core/math/vec2.h"
#include "game_common/match/world.h"
#include "world_sim.h"

// INFLUENCE TUNING
static constexpr ui8 INFLUENCE_SOURCE_LEVEL = 255; // Influence held by a settlement's own tile.
static constexpr i64 INFLUENCE_PRESSURE_ONE = 256; // Pressure fixed-point scale; pressure is clamped to [0, this].
static constexpr i32 INFLUENCE_GROW_DIV = 8; // Interior growth per tick = (strongest same-owner neighbour - own) * pressure / ONE / this.
static constexpr ui8 CLAIM_MIN_INFLUENCE = 32; // A border tile needs at least this much influence to claim a neighbour.
static constexpr ui8 CLAIM_START_INFLUENCE = 16; // Influence of a freshly claimed tile.
static constexpr ui64 INFLUENCE_FALL_BASE = 4; // Over budget, an adjacent plains tile at full (256/256) overshoot loses this much per tick. Scales with tile cost and overshoot.

// Maintenance cost of a tile, in 1/MAINTENANCE_COST_FP units:
// resistance * FP * (DIST_BASE_SQ + squared euclidean dist to settlement) / DIST_BASE_SQ * COST_SCALE.
// Capacity is compared as capacity * COST_FP. maintenance_totals are in the same fixed point.
static constexpr ui64 MAINTENANCE_COST_FP = 16;
static constexpr ui64 MAINTENANCE_DIST_BASE_SQ = 128;
static constexpr ui64 MAINTENANCE_COST_SCALE = 1;

// Terrain cost of a tile (maintenance), indexed by TERRAIN_TILE_TYPE.
static constexpr i32 TERRAIN_RESISTANCE[] =
{
    1,  // LAND_PLAINS
    2,  // LAND_HILLS
    8, // LAND_MOUNTAINS
    1,  // WATER_COAST
    3, // WATER_SEA
};

// Interior growth is slowed by tile cost: growth *= GROW_REF_COST / tile cost (factor never above 1). 0 disables it.
static constexpr i32 GROW_COST_SCALING = 1;
// Fixed-point cost of a plains tile adjacent to its settlement (influence_tile_cost(LAND_PLAINS, 1)).
static constexpr ui64 GROW_REF_COST = (ui64)TERRAIN_RESISTANCE[LAND_PLAINS] * MAINTENANCE_COST_FP * (MAINTENANCE_DIST_BASE_SQ + 1) * MAINTENANCE_COST_SCALE / MAINTENANCE_DIST_BASE_SQ;
// Land output of a tile at full influence (255), in population supported, indexed by TERRAIN_TILE_TYPE.
static constexpr ui32 TERRAIN_OUTPUT[] =
{
    3, // LAND_PLAINS
    2,  // LAND_HILLS
    1,  // LAND_MOUNTAINS
    30, // WATER_COAST
    2, // WATER_SEA
};

static constexpr i32 TILE_REGION_SIZE = world_terrain::REGION_SIZE;

// Region and regional index of the tile at world coordinates (must be inside the world).
static inline world_terrain::region& influence_tile_region(world_terrain& terrain, i32 x, i32 y, ui16& out_tile)
{
    out_tile = (ui16)((y % TILE_REGION_SIZE) * TILE_REGION_SIZE + x % TILE_REGION_SIZE);
    return terrain.regions[(y / TILE_REGION_SIZE) * terrain.size_regions.x + x / TILE_REGION_SIZE];
}

// Fixed-point maintenance cost of owning a tile of the given terrain at the given squared distance from its settlement.
// The single cost helper: claim check, unclaim and the land output pass all use it.
static inline ui64 influence_tile_cost(TERRAIN_TILE_TYPE type, ui64 dist_sq)
{
    return (ui64)(TERRAIN_RESISTANCE[type] + (MAINTENANCE_COST_FP * dist_sq / MAINTENANCE_DIST_BASE_SQ)) * MAINTENANCE_COST_SCALE;
}

// Recomputes the BORDER flag of one tile (reads only owners, so refresh order does not matter).
// An owned tile is border if it touches an unowned in-world 8-neighbour, or a neighbour owned by a settlement with a higher slot.
// At a contact line between two settlements only the lower-slot side is therefore flagged.
static void influence_refresh_border(world_terrain& terrain, i32 x, i32 y)
{
    ui16 tile;
    world_terrain::region& region = influence_tile_region(terrain, x, y, tile);
    const ui16 owner = region.owners[tile];
    bool border = false;

    if (owner != world_terrain::UNOWNED_TILE)
    {
        for (i32 dy = -1; dy <= 1 && !border; dy++)
        {
            for (i32 dx = -1; dx <= 1; dx++)
            {
                const i32 nx = x + dx;
                const i32 ny = y + dy;
                if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= terrain.size_tiles.x || ny >= terrain.size_tiles.y) continue;

                ui16 nTile;
                const ui16 nOwner = influence_tile_region(terrain, nx, ny, nTile).owners[nTile];
                if (nOwner == world_terrain::UNOWNED_TILE || nOwner > owner)
                {
                    border = true;
                    break;
                }
            }
        }
    }

    if (border) region.tile_flags[tile] |= TILE_FLAG_BORDER;
    else region.tile_flags[tile] &= ~TILE_FLAG_BORDER;
}

// Refreshes the BORDER flag of a tile and its 8 neighbours. Called after the owner of the tile changed.
static void influence_refresh_border_around(world_terrain& terrain, i32 x, i32 y)
{
    for (i32 dy = -1; dy <= 1; dy++)
    {
        for (i32 dx = -1; dx <= 1; dx++)
        {
            const i32 nx = x + dx;
            const i32 ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= terrain.size_tiles.x || ny >= terrain.size_tiles.y) continue;
            influence_refresh_border(terrain, nx, ny);
        }
    }
}

// Claim / maintain automaton over the tile planes, once per tick, in fixed region / row-major order.
// Owners, flags and claimed tiles' influence are mutated in place (first claimer in scan order wins).
// Influence growth reads old values and writes influence_next, copied back at the end, so growth has no scan direction bias.
void world_system_economy_influence_spread(match_world_state& world)
{
    world_terrain& terrain = world.terrain;
    world_entity_settlements& settlements = world.entities.settlements;
    const i32 regionsX = terrain.size_regions.x;
    const i32 regionsY = terrain.size_regions.y;
    const i32 tilesX = terrain.size_tiles.x;
    const i32 tilesY = terrain.size_tiles.y;

    // Per slot, how far over budget the maintenance total is, in 1/256 of capacity (1..256), 0 if within budget.
    // Computed once here from the tally as it is now, so the whole scan sees the same value.
    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        const ui64 capacityFp = settlements.guids[settlementIndex].is_valid() ? (ui64)settlements.influence_capacities[settlementIndex] * MAINTENANCE_COST_FP : 0;
        const ui64 total = settlements.maintenance_totals[settlementIndex];
        ui64 overshoot = 0;
        if (total > capacityFp)
        {
            overshoot = (total - capacityFp) * INFLUENCE_PRESSURE_ONE / (capacityFp > 1 ? capacityFp : 1);
            if (overshoot > INFLUENCE_PRESSURE_ONE) overshoot = INFLUENCE_PRESSURE_ONE;
            if (overshoot < 1) overshoot = 1; // Marks over budget even when the ratio truncates to 0.
        }
        settlements.maintenance_overshoots[settlementIndex] = (ui16)overshoot;
    }

    // Sources: a settlement whose tile is not yet its own claims it (new settlement), border flags included.
    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;

        const world_location loc = settlements.locations[settlementIndex];
        ui16 tile;
        world_terrain::region& region = influence_tile_region(terrain, loc.x, loc.y, tile);
        if (region.owners[tile] == settlementIndex) continue;

        region.owners[tile] = (ui16)settlementIndex;
        region.influence_levels[tile] = INFLUENCE_SOURCE_LEVEL;
        influence_refresh_border_around(terrain, loc.x, loc.y);
    }

    for (i32 regionY = 0; regionY < regionsY; regionY++)
    {
        for (i32 regionX = 0; regionX < regionsX; regionX++)
        {
            world_terrain::region& region = terrain.regions[regionY * regionsX + regionX];

            for (i32 tileY = 0; tileY < TILE_REGION_SIZE; tileY++)
            {
                for (i32 tileX = 0; tileX < TILE_REGION_SIZE; tileX++)
                {
                    const ui16 tile = (ui16)(tileY * TILE_REGION_SIZE + tileX);
                    const ui16 owner = region.owners[tile];

                    // Unowned tiles (never flagged border) hold nothing.
                    if (owner == world_terrain::UNOWNED_TILE)
                    {
                        region.influence_next[tile] = 0;
                        region.contestation_levels[tile] = 0;
                        region.tile_flags[tile] &= ~(TILE_FLAGS::TILE_FLAG_BORDER);
                        continue;
                    }

                    const i32 worldX = regionX * TILE_REGION_SIZE + tileX;
                    const i32 worldY = regionY * TILE_REGION_SIZE + tileY;
                    const world_location ownerLoc = settlements.locations[owner];

                    // A destroyed settlement has no capacity, so its whole field decays.
                    const i64 capacity = settlements.guids[owner].is_valid() ? (i64)settlements.influence_capacities[owner] : 0;
                    const ui64 capacityFp = (ui64)capacity * MAINTENANCE_COST_FP;
                    const ui64 overshoot = settlements.maintenance_overshoots[owner]; // Fixed for the whole tick, 0 = within budget.
                    const bool overBudget = overshoot > 0;

                    i32 influence = region.influence_levels[tile];

                    // Interior growth towards the strongest same-owner neighbour, driven by how far output is under capacity.
                    // Frozen while over budget so it cannot fight the border erosion.
                    i64 pressure = 0;
                    if (!overBudget)
                    {
                        pressure = (capacity - (i64)settlements.land_outputs[owner]) * INFLUENCE_PRESSURE_ONE / (capacity > 1 ? capacity : 1);
                        if (pressure < 0) pressure = 0;
                        if (pressure > INFLUENCE_PRESSURE_ONE) pressure = INFLUENCE_PRESSURE_ONE;
                    }
                    if (pressure > 0)
                    {
                        i32 strongest = 0;
                        for (i32 dy = -1; dy <= 1; dy++)
                        {
                            for (i32 dx = -1; dx <= 1; dx++)
                            {
                                const i32 nx = worldX + dx;
                                const i32 ny = worldY + dy;
                                if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= tilesX || ny >= tilesY) continue;

                                ui16 nTile;
                                const world_terrain::region& nRegion = influence_tile_region(terrain, nx, ny, nTile);
                                if (nRegion.owners[nTile] == owner && nRegion.influence_levels[nTile] > strongest) strongest = nRegion.influence_levels[nTile];
                            }
                        }
                        if (strongest > influence)
                        {
                            // delta = diff * pressure / ONE / GROW_DIV * GROW_REF_COST / tileCost, as one division to limit truncation.
                            // Tile cost is never below GROW_REF_COST, so the cost factor never exceeds 1.
                            ui64 num = (ui64)(strongest - influence) * pressure;
                            ui64 den = (ui64)INFLUENCE_PRESSURE_ONE * INFLUENCE_GROW_DIV;
                            if (GROW_COST_SCALING)
                            {
                                num *= GROW_REF_COST;
                                den *= influence_tile_cost(region.terrain_types[tile], vec2_dist_squared(world_location{worldX, worldY}, ownerLoc));
                            }
                            influence += (i32)(num / den);
                        }
                        if (influence > 255) influence = 255;
                    }

                    if (overBudget)
                    {
                        // Over budget: every owned tile except the settlement tile decays in proportion to its own cost,
                        // so costly / far tiles go first. fall is at least 1 so a tiny overshoot cannot stall the decay.
                        if (!(worldX == ownerLoc.x && worldY == ownerLoc.y && settlements.guids[owner].is_valid()))
                        {
                            const ui64 tileCost = influence_tile_cost(region.terrain_types[tile], vec2_dist_squared(world_location{worldX, worldY}, ownerLoc));
                            ui64 fall = INFLUENCE_FALL_BASE * tileCost * overshoot / (GROW_REF_COST * INFLUENCE_PRESSURE_ONE);
                            if (fall < 1) fall = 1;
                            if (fall > (ui64)influence) fall = influence;
                            influence -= (i32)fall;
                            if (influence == 0)
                            {
                                // Unclaimed: neighbours of the same owner become border.
                                settlements.maintenance_totals[owner] -= tileCost;
                                region.owners[tile] = world_terrain::UNOWNED_TILE;
                                region.influence_next[tile] = 0;
                                region.contestation_levels[tile] = 0;
                                influence_refresh_border_around(terrain, worldX, worldY);
                                continue;
                            }
                        }
                    }
                    else if ((region.tile_flags[tile] & TILE_FLAG_BORDER) && influence >= CLAIM_MIN_INFLUENCE)
                    {
                            // Claim the cheapest affordable unowned neighbour (ties: first in fixed order).
                            ui64 bestCost = ~(ui64)0;
                            i32 bestX = 0, bestY = 0;
                            for (i32 dy = -1; dy <= 1; dy++)
                            {
                                for (i32 dx = -1; dx <= 1; dx++)
                                {
                                    const i32 nx = worldX + dx;
                                    const i32 ny = worldY + dy;
                                    if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= tilesX || ny >= tilesY) continue;

                                    ui16 nTile;
                                    const world_terrain::region& nRegion = influence_tile_region(terrain, nx, ny, nTile);
                                    if (nRegion.owners[nTile] != world_terrain::UNOWNED_TILE) continue;

                                    const ui64 cost = influence_tile_cost(nRegion.terrain_types[nTile], vec2_dist_squared(world_location{nx, ny}, ownerLoc));
                                    if (settlements.maintenance_totals[owner] + cost > capacityFp || cost >= bestCost) continue;

                                    bestCost = cost;
                                    bestX = nx;
                                    bestY = ny;
                                }
                            }

                            if (bestCost != ~(ui64)0)
                            {
                                ui16 nTile;
                                world_terrain::region& nRegion = influence_tile_region(terrain, bestX, bestY, nTile);
                                settlements.maintenance_totals[owner] += bestCost;
                                nRegion.owners[nTile] = owner;
                                nRegion.influence_levels[nTile] = CLAIM_START_INFLUENCE;
                                nRegion.influence_next[nTile] = CLAIM_START_INFLUENCE;
                                influence_refresh_border_around(terrain, bestX, bestY);
                            }
                    }

                    region.influence_next[tile] = (ui8)influence;
                }
            }
        }
    }

    // Source tiles always hold full influence.
    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;

        const world_location loc = settlements.locations[settlementIndex];
        ui16 tile;
        influence_tile_region(terrain, loc.x, loc.y, tile).influence_next[tile] = INFLUENCE_SOURCE_LEVEL;
    }

    // Copy the new influence back.
    for (i32 regionIndex = 0; regionIndex < regionsX * regionsY; regionIndex++)
    {
        world_terrain::region& region = terrain.regions[regionIndex];
        ia_memcpy(region.influence_levels, region.influence_next, sizeof(region.influence_levels));
    }
}

// Land output and maintenance of each settlement, summed over its owned tiles in one pass.
// Output is terrain output weighted by influence, capped by the settlement's capacity. It feeds the population target in world_system_settlement_economy.
// Maintenance is the tile cost sum, read by the influence spread system next tick.
void world_system_economy_land_output(match_world_state& world)
{
    world_terrain& terrain = world.terrain;
    world_entity_settlements& settlements = world.entities.settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        settlements.land_output_sums[settlementIndex] = 0;
        settlements.maintenance_totals[settlementIndex] = 0;
    }

    for (i32 regionY = 0; regionY < terrain.size_regions.y; regionY++)
    {
        for (i32 regionX = 0; regionX < terrain.size_regions.x; regionX++)
        {
            const world_terrain::region& region = terrain.regions[regionY * terrain.size_regions.x + regionX];
            for (ui16 tile = 0; tile < world_terrain::REGION_TILE_COUNT; tile++)
            {
                const ui16 owner = region.owners[tile];
                if (owner == world_terrain::UNOWNED_TILE) continue;

                const i32 worldX = regionX * TILE_REGION_SIZE + tile % TILE_REGION_SIZE;
                const i32 worldY = regionY * TILE_REGION_SIZE + tile / TILE_REGION_SIZE;
                const TERRAIN_TILE_TYPE type = region.terrain_types[tile];

                settlements.land_output_sums[owner] += (ui64)TERRAIN_OUTPUT[type] * region.influence_levels[tile];
                settlements.maintenance_totals[owner] += influence_tile_cost(type, vec2_dist_squared(world_location{ worldX, worldY }, settlements.locations[owner]));
            }
        }
    }

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        const ui64 capacity = settlements.guids[settlementIndex].is_valid() ? settlements.influence_capacities[settlementIndex] : 0;
        const ui64 output = settlements.land_output_sums[settlementIndex] / 255;
        settlements.land_outputs[settlementIndex] = (ui32)(output < capacity ? output : capacity);
    }
}

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

        ui32& population = settlements.populations[settlementIndex];
        
        i32 growth = ((i64)targetPopulation - (i64)population) * 5 / 100;
        if (growth > 0 && (ui32)growth >= 1 + population / 100)
        {
            growth = 1 + (i64)population / 100; // Population cannot grow by more than 1% per tick.
        }

        if (population + growth < 1)
        {
            population = 1;
            continue;
        }

        population += growth;

        // Calculate new influence capacity.
        settlements.influence_capacities[settlementIndex] = population + 10 + population / 2;
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
