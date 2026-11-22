// Pathfinding: SKELETON. The generalist pathfinder and the world's path store are stubs, to be written from scratch.

#include "game_common/match/world/world_pathing.h"
#include "core/memory.h"
#include "game_common/match/world/world_state.h"
#include "game_common/match/world/world_terrain.h"
#include "world_movement.h"

// Initializes the areas in the grid cell at the provided coordinates.
// Places the new areas in the provided memory, and sets the area id on the tile_areas collection.
static void world_path_store_init_grid_cell_areas(const world_terrain& terrain, world_path_areas* areas,
        ui16 grid_x, ui16 grid_y, path_area_id& next_area_id)
{
    enum FLOODFILL_TILE_STATE : ui8
    {
        UNEXPLORED,
        OPEN,
        CLOSED,
    } floodfillBuffer[16 * 16]; // Flags buffer for determining what tiles are open / remain to be evaluated
                                // for addition to area being built.

    for (ui16 tileY = 0; tileY < 16; tileY++)
    {
        for (ui16 tileX = 0; tileX < 16; tileX++)
        {
            world_location originTileLoc = { grid_x * 16 + tileX, grid_y * 16 + tileY };
            
            ui16 originRegionIndex, originTileIndex;
            world_get_tile(terrain, originTileLoc, originRegionIndex, originTileIndex);

            ui32 originTileWorldIndex = originRegionIndex * world_terrain::REGION_TILE_COUNT + originTileIndex;
            if (areas->tile_areas[originTileWorldIndex] != 0) continue;
            
            // Create new area starting on this tile.
            areas->tile_areas[originTileWorldIndex] = next_area_id;
            areas->types[next_area_id] = terrain_is_land(terrain.regions[originRegionIndex].terrain_types[originTileIndex]) ?
                                        PATH_AREA_TYPE::LAND : PATH_AREA_TYPE::SEA;
            next_area_id++;

            // Reinit floodfill buffer. Open origin tile.
            ia_memzero(floodfillBuffer, sizeof(floodfillBuffer));
            floodfillBuffer[tileY * 16 + tileX] = OPEN;

            // Begin flood-filling.
            ui16 openTileCount = 1;
            ui16 areaSize = 1;

            for (;openTileCount > 0;)
            {
                for (ui16 floodTileIndex = 0; floodTileIndex < 256; floodTileIndex++)
                {
                    if (floodfillBuffer[floodTileIndex] != OPEN) continue;

                    floodfillBuffer[floodTileIndex] = CLOSED;
                    openTileCount--;

                    world_location tileLoc = { grid_x * 16 + floodTileIndex % 16,
                            grid_y * 16 + floodTileIndex / 16 };

                    ui16 regionIndex, tileIndex;
                    world_get_tile(terrain, tileLoc, regionIndex, tileIndex);

                    ui32 tileWorldIndex = regionIndex * world_terrain::REGION_TILE_COUNT + tileIndex;

                    const TERRAIN_TILE_TYPE& tileTerrainType = terrain.regions[regionIndex].terrain_types[tileIndex];
                    // We assume the path grid to be aligned with regions, meaning we can assume region index
                    // is the same, and that neighbours can be found directly with region size arithmetic.

                    // WEST
                    if (tileLoc.x > grid_x * 16
                            && floodfillBuffer[floodTileIndex - 1] == UNEXPLORED)
                    {
                        const TERRAIN_TILE_TYPE& neighbourTerrainType = terrain.regions[regionIndex].terrain_types[tileIndex - 1];

                        if (terrain_is_land(tileTerrainType) == terrain_is_land(neighbourTerrainType))
                        {
                            floodfillBuffer[floodTileIndex - 1] = OPEN;
                            openTileCount++;
                            areaSize++;

                            ui32 neighbourWorldIndex = tileWorldIndex - 1;
                            areas->tile_areas[neighbourWorldIndex] = areas->tile_areas[tileWorldIndex];
                        }
                        else
                        {
                            floodfillBuffer[floodTileIndex - 1] = CLOSED;
                        }
                    }
                    // EAST
                    if (tileLoc.x < grid_x * 16 + 15
                            && floodfillBuffer[floodTileIndex + 1] == UNEXPLORED)
                    {
                        const TERRAIN_TILE_TYPE& neighbourTerrainType = terrain.regions[regionIndex].terrain_types[tileIndex + 1];

                        if (terrain_is_land(tileTerrainType) == terrain_is_land(neighbourTerrainType))
                        {
                            floodfillBuffer[floodTileIndex + 1] = OPEN;
                            openTileCount++;
                            areaSize++;

                            ui32 neighbourWorldIndex = tileWorldIndex + 1;
                            areas->tile_areas[neighbourWorldIndex] = areas->tile_areas[tileWorldIndex];
                        }
                        else
                        {
                            floodfillBuffer[floodTileIndex + 1] = CLOSED;
                        }
                    }
                    // SOUTH
                    if (tileLoc.y > grid_y * 16
                            && floodfillBuffer[floodTileIndex - 16] == UNEXPLORED)
                    {
                        const TERRAIN_TILE_TYPE& neighbourTerrainType = 
                            terrain.regions[regionIndex].terrain_types[tileIndex - world_terrain::REGION_SIZE];

                        if (terrain_is_land(tileTerrainType) == terrain_is_land(neighbourTerrainType))
                        {
                            floodfillBuffer[floodTileIndex - 16] = OPEN;
                            openTileCount++;
                            areaSize++;

                            ui32 neighbourWorldIndex = tileWorldIndex - world_terrain::REGION_SIZE;
                            areas->tile_areas[neighbourWorldIndex] = areas->tile_areas[tileWorldIndex];
                        }
                        else
                        {
                            floodfillBuffer[floodTileIndex - 16] = CLOSED;
                        }
                    }
                    // NORTH
                    if (tileLoc.y < grid_y * 16 + 15
                            && floodfillBuffer[floodTileIndex + 16] == UNEXPLORED)
                    {
                        const TERRAIN_TILE_TYPE& neighbourTerrainType = 
                            terrain.regions[regionIndex].terrain_types[tileIndex + world_terrain::REGION_SIZE];

                        if (terrain_is_land(tileTerrainType) == terrain_is_land(neighbourTerrainType))
                        {
                            floodfillBuffer[floodTileIndex + 16] = OPEN;
                            openTileCount++;
                            areaSize++;

                            ui32 neighbourWorldIndex = tileWorldIndex + world_terrain::REGION_SIZE;
                            areas->tile_areas[neighbourWorldIndex] = areas->tile_areas[tileWorldIndex];
                        }
                        else
                        {
                            floodfillBuffer[floodTileIndex + 16] = CLOSED;
                        }
                    }
                }
            }

            DEBUG_LOGF("New path area created: %d. Grid %d, %d, Size = %d", next_area_id - 1, grid_x, grid_y, areaSize);
        }
    }
}

// Initializes area-to-area connections across cell boundaries
// Places the new connections in the provided memory.
static void world_path_store_init_grid_cell_area_connections(const match_world_state& world, world_path_areas& areas, mem_arena& mem, ui16 grid_x, ui16 grix_y)
{
    // @TODO(Marc): Pass a memory arena and fill it progressively in the same way the areas are built / filled in.
    // For each areas, scan all "edge tiles" (tiles that touch the grid cell edges) and determine if it touches a
    // valid area on the other side (same type, later may be relaxed).
    //
    // Temporarily, keep it simple by creating up to one connection to a given neighbour area over a single tile.
    // Later we'll want the ability to support any number of connections to a neighbour (in case it gets broken up over the edge),
    // and more importantly support multi-tile portal edges so entities don't have to path to a specific portal tile
    // to cross (is that relevant ? Could also be handled by the low level pathfinding being aware of current AND next
    // area, not just current area targeting portal).
}

// Updates the cost estimate for travelling between areas in the world.
// Runs a full scan through. @TODO(Marc): Batch ?
// The cost is a rough estimation based on travel cost between area "center mass" and the portal tile.
static void world_path_store_update_connection_costs(const match_world_state& world, world_path_areas& areas)
{

}

world_path_store* world_path_store_init(const match_world_state& world, mem_arena& memory)
{
    world_path_store* store = memory.alloc<world_path_store>();
    if (store == nullptr) return nullptr;

    // Allocate search scratch buffer.
    if (memory.mem_size - memory.allocated_count < WORLD_PATH_STORE_SCRATCH_SIZE) return nullptr;
    store->search_scratch = mem_arena_create_sub(memory, WORLD_PATH_STORE_SCRATCH_SIZE);

    static constexpr ui64 STORAGE_SIZE = MiB(4);
    if (memory.mem_size - memory.allocated_count < STORAGE_SIZE) return nullptr;

    // @TODO(Marc): Pre-calc required memory for storage and allocate exactly as needed.
    // For now we just allocate one size that we consider as "as much as we could possibly need".
    store->storage = mem_arena_create_sub(memory, STORAGE_SIZE);

    store->areas = store->storage.alloc<world_path_areas>();
    if (store->areas == nullptr) return nullptr;

    // Build path areas, connections and determine start high-level costs.
    const ui8 GRID_SIZE_MULT = (world_terrain::REGION_SIZE / 16);
    vec2<ui16> pathGridDims = {
        world.terrain->size_regions.x * GRID_SIZE_MULT,
        world.terrain->size_regions.y * GRID_SIZE_MULT
    };

    // Each grid cell covers 16 x 16 tiles.
    // For each tile, scan for an existing local component the tile could be part of.
    // If there are none, create a new one by picking the next component ID.

    path_area_id nextAreaID = 1; // Begin area IDs at 1 since 0 means none / invalid.

    // Alloc area data.
    store->areas->tile_areas = store->storage.alloc<path_area_id>(world.terrain->size_tiles.x * world.terrain->size_tiles.y);
    store->areas->types = store->storage.alloc<PATH_AREA_TYPE>(pathGridDims.x * pathGridDims.y * 2); // @TODO(Marc): Allocate as needed instead of predetermining uncertain size.
    ASSERT(store->areas->tile_areas != nullptr);
    ASSERT(store->areas->types != nullptr);

    ui32 worldTileCount = world.terrain->size_tiles.x * world.terrain->size_tiles.y;
    ia_memzero(store->areas->tile_areas, worldTileCount * sizeof(path_area_id));

    for (ui16 gridX = 0; gridX < pathGridDims.x; gridX++)
    {
        for (ui16 gridY = 0; gridY < pathGridDims.y; gridY++)
        {
            world_path_store_init_grid_cell_areas(*world.terrain, store->areas, 
                    gridX, gridY, nextAreaID);
        }
    }

    DEBUG_LOGF("Created %d path areas.", nextAreaID - 1);

    for (ui16 gridX = 0; gridX < pathGridDims.x; gridX++)
    {
        for (ui16 gridY = 0; gridY < pathGridDims.y; gridY++)
        {
            world_path_store_init_grid_cell_area_connections(world, *store->areas, store->storage, 
                    gridX, gridY);
        }
    }

    world_path_store_update_connection_costs(world, *store->areas);

    return store;
}


world_path_store::path_handle world_path_store_get_path(match_world_state& world, const path_params& parameters)
{
    // @TODO(Marc): Lookup path in cache that fits the passed parameters.
    return INVALID_PATH_HANDLE;
}

// General-purpose path finder. Has a read-only relationship with the path store.
void world_path_find(const match_world_state& world, const path_params& params, mem_arena& scratch, path& out_path)
{
    out_path.points = nullptr;
    out_path.point_count = 0;

    out_path.length = 0;
    out_path.cost = 0;

    // @TODO(Marc): Determine start and end areas. Then, three parts:
    // - Cache check. If an existing path exists between start and end areas, use it.
    // - If cache miss, perform high level path finding through areas.
    // - Perform low level path finding from start location and first portal tile.
}

bool world_path_store_move_entity(match_world_state& world, world_location& location, world_entity_movement& entity_movement)
{
    // @TODO(Marc): Get path stored within entity_movement structure and check its validity. If not,
    // return false and let the entity get a new one.
    // Use valid path and current entity location to determine next location given its travel speed.
    return false;
}

world_location world_path_store_next_tile(match_world_state& world, world_path_store::path_handle path_handle, world_location location, world_location destination)
{
    // @TODO(Marc): Determine path handle validity. Return false if not valid, and let the caller get a new path.
    // Use valid path and current location to get next location along the path.
    return location;
}
