// Terrain generation.

#include "game_common/match/world.h"
#include "world_sim.h"

// Generates terrain: ring of mountains then hills along the world border, plains elsewhere.
// TODO(Marc): Replace with map generation from data transmitted by the server before match start (e.g. image files).
void world_terrain_generate(world_terrain& terrain)
{
    const ui16 MOUNTAIN_BORDER_DISTANCE = 10;
    const ui16 HILLS_BORDER_DISTANCE = 15;
    const i32 LAKE_HALF_SIZE = 4; // Central lake spans tiles [center - 5, center + 4] on both axes (10x10).
    const i32 LAKE_COAST_THICKNESS = 2;

    for (ui16 y = 0; y < terrain.size_tiles.y; y++)
    {
        for (ui16 x = 0; x < terrain.size_tiles.x; x++)
        {
            // Distance to the nearest world border.
            ui16 d = x;
            if (y < d) d = y;
            if (terrain.size_tiles.x - 1 - x < d) d = terrain.size_tiles.x - 1 - x;
            if (terrain.size_tiles.y - 1 - y < d) d = terrain.size_tiles.y - 1 - y;

            TERRAIN_TILE_TYPE type = TERRAIN_TILE_TYPE::LAND_PLAINS;
            if (d < MOUNTAIN_BORDER_DISTANCE) type = TERRAIN_TILE_TYPE::LAND_MOUNTAINS;
            else if (d < HILLS_BORDER_DISTANCE) type = TERRAIN_TILE_TYPE::LAND_HILLS;
            else
            {
                // Distance from the lake's outer edge, inward (negative = outside the lake).
                i32 lakeMinX = terrain.size_tiles.x / 2 - LAKE_HALF_SIZE;
                i32 lakeMinY = terrain.size_tiles.y / 2 - LAKE_HALF_SIZE;
                i32 lakeDepth = x - lakeMinX;
                if (lakeMinX + 2 * LAKE_HALF_SIZE - 1 - x < lakeDepth) lakeDepth = lakeMinX + 2 * LAKE_HALF_SIZE - 1 - x;
                if ((i32)y - lakeMinY < lakeDepth) lakeDepth = (i32)y - lakeMinY;
                if (lakeMinY + 2 * LAKE_HALF_SIZE - 1 - (i32)y < lakeDepth) lakeDepth = lakeMinY + 2 * LAKE_HALF_SIZE - 1 - (i32)y;

                if (lakeDepth >= LAKE_COAST_THICKNESS) type = TERRAIN_TILE_TYPE::WATER_SEA;
                else if (lakeDepth >= 0) type = TERRAIN_TILE_TYPE::WATER_COAST;
            }

            // Regions are stored row-major, and tiles are row-major within a region.
            ui32 regionIndex = (y / world_terrain::REGION_SIZE) * terrain.size_regions.x + (x / world_terrain::REGION_SIZE);
            ui32 tileIndex = (y % world_terrain::REGION_SIZE) * world_terrain::REGION_SIZE + (x % world_terrain::REGION_SIZE);
            terrain.regions[regionIndex].terrain_types[tileIndex] = type;

            // Also initialize tile ownership to UNOWNED and border count to 4.
            terrain.regions[regionIndex].owners[tileIndex] = world_terrain::UNOWNED_TILE;
            terrain.regions[regionIndex].status[tileIndex].border_count = 4;
        }
    }
}
