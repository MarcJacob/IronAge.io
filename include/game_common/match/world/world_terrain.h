
#ifndef WORLD_TERRAIN_INCLUDED
#define WORLD_TERRAIN_INCLUDED

#include "world.h"

// Types of terrain tiles.
enum class TERRAIN_TILE_TYPE : ui8
{
	LAND_PLAINS,
	LAND_HILLS,
	LAND_MOUNTAINS,

	WATER_COAST,
	WATER_SEA,
};

// Holds the state of a world's terrain / tiles.
struct world_terrain
{
	world_dimensions size_tiles; // Size of the world in tiles.
	world_dimensions size_regions; // Size of the world in regions.

	static constexpr ui8 REGION_SIZE = 128; // Square side size of a region. Total tile count per region = REGION_SIZE^2.
	static constexpr ui16 REGION_TILE_COUNT = REGION_SIZE * REGION_SIZE;

	// A Region is a square of REGION_SIZE * REGION_SIZE tiles. Each of the world's dimensions must be a multiplier of that size.
	// Tiles are stored square-by-square as to increase data locality in most cases where we'll want to apply area-based effects.
	struct region
	{
        // Geologic / static properties determined at terrain generation / authoring.
		TERRAIN_TILE_TYPE terrain_types[REGION_TILE_COUNT]; // Type of terrain of this tile. Influences all entities and mechanics.

        // Dynamic properties
        struct
        {
            ui8 border_count : 3; // How many neighbouring tiles are unowned or owned by a different settlements ? Tile is considered to be a border if above 0.
            bool is_road : 1;
            bool distance_recalc : 1; // Whether this tile's distance to settlement may be lowered by its neighbours (never raised: see territories' distances_stale).
            bool takeover_check : 1; // Whether this tile (or a neighbour) changed owner or distance since a neighbouring settlement last checked for taking it over.
            // ...
        } status[REGION_TILE_COUNT]; // Various status values packed together in a byte.
        ui16 owners[REGION_TILE_COUNT]; // Tile owner if any, as a settlement slot index (see UNOWNED_TILE). Determines which settlement obtains the tile's production output.
        ui16 distances[REGION_TILE_COUNT]; // Travel distances to settlement value for claimed tiles.
    } *regions;

    static constexpr ui16 UNOWNED_TILE = ~0; // Owner value of a tile with no owner (influence 0).
};

// Returns region index & tile regional index of tile at given world location.
// Asserts if the location is invalid !
// @NOTE(Marc): Do not use this in performance-critical systems if possible. Instead, apply logic on every tile region-by-region.
static inline void world_get_tile(const world_terrain& terrain, world_location loc, ui16& out_region_index, ui16& out_tile_index)
{
    ASSERT(loc.x >= 0 && loc.x < terrain.size_tiles.x);
    ASSERT(loc.y >= 0 && loc.y < terrain.size_tiles.y);

    ui16 regionX = loc.x / world_terrain::REGION_SIZE;
    ui16 regionY = loc.y / world_terrain::REGION_SIZE;

    ui16 tileX = loc.x - regionX * world_terrain::REGION_SIZE;
    ui16 tileY = loc.y - regionY * world_terrain::REGION_SIZE;

    out_region_index = regionY * terrain.size_regions.x + regionX;
    out_tile_index = tileY * world_terrain::REGION_SIZE + tileX;
}

static inline world_location world_get_tile_location(const world_terrain& terrain, ui16 region_index, ui16 tile_index)
{
    ASSERT(region_index < (terrain.size_regions.x * terrain.size_regions.y));
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    return { (region_index % terrain.size_regions.x * world_terrain::REGION_SIZE) + (tile_index % world_terrain::REGION_SIZE),
            (region_index / terrain.size_regions.x * world_terrain::REGION_SIZE) + (tile_index / world_terrain::REGION_SIZE)};
}

// Gets the region and tile indices of the tile at the given offset from a world location. Returns false if that tile is off the map.
inline static bool world_get_neighbour_tile(const world_terrain& terrain, world_location location, vec2<i8> offset, ui16& out_region_index, ui16& out_tile_index)
{
    i32 x = location.x + offset.x;
    i32 y = location.y + offset.y;
    if (x < 0 || x >= terrain.size_tiles.x || y < 0 || y >= terrain.size_tiles.y) return false;

    world_get_tile(terrain, { (ui16)x, (ui16)y }, out_region_index, out_tile_index);
    return true;
}

// Identifies a path kept by the world's path store. Placeholder: always invalid until the store can keep several paths.
static constexpr ui32 INVALID_PATH_HANDLE = 0;

#endif // WORLD_TERRAIN_INCLUDED
