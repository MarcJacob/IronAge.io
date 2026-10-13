// Tile travel costs and neighbour helpers shared by the proximity (economy) and road pathfinding code.

#ifndef WORLD_TILE_TRAVEL_INCLUDED
#define WORLD_TILE_TRAVEL_INCLUDED

#include "core/math/vec2.h"
#include "game_common/match/world.h"

// Proximity / travel difficulty over each terrain type.
static const ui8 TILE_TERRAIN_TRAVEL_COST[] = {
    3, // LAND_PLAINS
    5, // LAND_HILLS
    10, // LAND_MOUNTAINS

    1, // WATER_COAST
    4, // WATER_SEA
};

// Travel cost of a tile with a road, replacing its terrain cost. Must stay below every terrain cost. @PLACEHOLDER(tune)
static constexpr ui8 TILE_ROAD_TRAVEL_COST = 1;

static constexpr ui8 TILE_WILDERNESS_EXTRA_COST = 10;

// Proximity is measured with chamfer weights (integer approximation of euclidean distance, ratio 7 / 5 = 1.4 ~ sqrt(2)).
// Stepping to a tile adds its travel cost times the weight.
static constexpr ui16 PROXIMITY_WEIGHT_ORTHOGONAL = 5;
static constexpr ui16 PROXIMITY_WEIGHT_DIAGONAL = 7;

// Neighbour offsets, non-diagonal only or all 8.
static const vec2<i8> NEIGHBOUR_OFFSETS_ORTHOGONAL[] = { {-1, 0}, {0, -1}, {0, 1}, {1, 0} };
static const vec2<i8> NEIGHBOUR_OFFSETS_ALL[] = { {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1} };

// Travel cost of a tile: the road cost if it has a road, its terrain cost otherwise.
static inline ui8 world_get_tile_travel_cost(const world_terrain& terrain, ui16 region_index, ui16 tile_index)
{
    if (terrain.regions[region_index].status[tile_index].is_road) return TILE_ROAD_TRAVEL_COST;
    return TILE_TERRAIN_TRAVEL_COST[(ui8)terrain.regions[region_index].terrain_types[tile_index]];
}

// Gets the region and tile indices of the tile at the given offset from a world location. Returns false if that tile is off the map.
static inline bool world_get_neighbour_tile(const world_terrain& terrain, world_location location, vec2<i8> offset, ui16& out_region_index, ui16& out_tile_index)
{
    i32 x = location.x + offset.x;
    i32 y = location.y + offset.y;
    if (x < 0 || x >= terrain.size_tiles.x || y < 0 || y >= terrain.size_tiles.y) return false;

    world_get_tile(terrain, { (ui16)x, (ui16)y }, out_region_index, out_tile_index);
    return true;
}

#endif // WORLD_TILE_TRAVEL_INCLUDED
