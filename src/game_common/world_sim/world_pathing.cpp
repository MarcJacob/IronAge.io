// Pathfinding: SKELETON. The generalist pathfinder and the world's path store are stubs, to be written from scratch.

#include "game_common/match/world/world_state.h"
#include "game_common/match/world/world_terrain.h"
#include "world_movement.h"

// Possible types for a path area.
// For now, binary. Later we may want to break it down into something more granular, in case we want to prevent
// some entities from crossing mountains, or allow others to only navigate on coastlines, and so on.
enum class PATH_AREA_TYPE : ui8
{
    LAND,
    SEA,
};

// Uniquely identifies a single path area, which occupies up to one entire path grid cell.
// Maps to a type and a world path component index. Its connections can be queried using it as well.
using path_area_id = ui16;

// Monodirectional connection between an origin area (infered from connections storage) and a destination area.
// Contains the "portal tile" expressed as a world location.
struct path_area_connection
{
    path_area_id dest_area;
    world_location portal_tile;
    ui16 travel_cost; // Travel cost used when going through this connection in high-level pathfinding.
                      // Estimated from roughly how costly it is to reach some center-mass tile of the area to the portal tile.
};

// Stores Path Areas which are local pathing components with land or sea types, located in Path Regions
// (init-time grid of 16x16 tiles cells covering the entire world). Areas are stored grid-width-wise, in order of local index.
struct world_path_areas
{
    path_area_id* tile_areas; // X-major, region by region (uses the same world location -> region / tile index as terrain tiles).
    PATH_AREA_TYPE* types;
    path_area_connection* connections;
};

// Initializes the areas in the grid cell at the provided coordinates.
// Places the new areas in the provided memory, and sets the area id on the tile_areas collection.
static void world_path_store_init_grid_cell_areas(const match_world_state& world, mem_arena& mem, path_area_id* tile_areas,
        ui16 grid_x, ui16 grid_y, path_area_id& next_area_id)
{
    world_location cellStart = { grid_x * 16, grid_y * 16 };
    world_location cellEnd = cellStart + world_location { 15, 15 };

    for (ui16 tileX = cellStart.x; tileX < cellEnd.x; tileX++)
    {
        for (ui16 tileY = cellStart.y; tileY < cellEnd.y; tileY++)
        {
            world_location tileLoc = { tileX, tileY };
            
            ui16 regionIndex, tileIndex;
            world_get_tile(*world.terrain, tileLoc, regionIndex, tileIndex);

            ui32 tileWorldIndex = regionIndex * world_terrain::REGION_TILE_COUNT + tileIndex;
            if (tile_areas[tileWorldIndex] == 0)
            {
                // Create new area starting on this tile.
                tile_areas[tileWorldIndex] = next_area_id++;
            }

            // Based on this tile's terrain, spread the area east and northward (positive on both axis) if they share a similar terrain.
            const TERRAIN_TILE_TYPE& terrainType = world.terrain->regions[regionIndex].terrain_types[tileIndex];
            
            if (tileX < cellEnd.x - 1)
            {
                const TERRAIN_TILE_TYPE& terrainTypeEast = world.terrain->regions[regionIndex].terrain_types[tileIndex + 1];
                if (terrain_is_land(terrainType) == terrain_is_land(terrainTypeEast))
                {
                    tile_areas[tileWorldIndex + 1] = tile_areas[tileWorldIndex];
                }
            }

            if (tileY < cellEnd.y - 1)
            {
                const TERRAIN_TILE_TYPE& terrainTypeNorth = world.terrain->regions[regionIndex].terrain_types[tileIndex + world_terrain::REGION_SIZE];
                if (terrain_is_land(terrainType) == terrain_is_land(terrainTypeNorth))
                {
                    tile_areas[tileWorldIndex + world_terrain::REGION_SIZE] = tile_areas[tileWorldIndex];
                }
            }
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

    world_path_areas* areas = store->storage.alloc<world_path_areas>();
    if (areas == nullptr) return nullptr;

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

    // Alloc tile area IDs.
    areas->tile_areas = (path_area_id*)store->storage.alloc(world.terrain->size_tiles.x * world.terrain->size_tiles.y);

    for (ui16 gridX = 0; gridX < pathGridDims.x; gridX++)
    {
        for (ui16 gridY = 1; gridY < pathGridDims.y; gridY++)
        {
            world_path_store_init_grid_cell_areas(world, store->storage, areas->tile_areas, gridX, gridY, nextAreaID);
        }
    }

    for (ui16 gridX = 0; gridX < pathGridDims.x; gridX++)
    {
        for (ui16 gridY = 1; gridY < pathGridDims.y; gridY++)
        {
            world_path_store_init_grid_cell_area_connections(world, *areas, store->storage, gridX, gridY);
        }
    }


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
