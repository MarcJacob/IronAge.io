// Terrain generation.

#include "game_common/match/world/world_terrain.h"
#include "world_sim.h"

// Initializes the dynamic properties of all tiles: unowned, border count of 4.
void world_terrain_init_tile_status(world_terrain& terrain)
{
    ui32 regionCount = terrain.size_regions.x * terrain.size_regions.y;
    for (ui32 regionIndex = 0; regionIndex < regionCount; regionIndex++)
    {
        for (ui32 tileIndex = 0; tileIndex < world_terrain::REGION_TILE_COUNT; tileIndex++)
        {
            terrain.regions[regionIndex].owners[tileIndex] = world_terrain::UNOWNED_TILE;
            terrain.regions[regionIndex].status[tileIndex].border_count = 4;
        }
    }
}
