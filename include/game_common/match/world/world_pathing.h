// Public pathfinding symbols: the generalist pathfinder over the world's terrain.
// SKELETON: being rewritten from scratch, the functions are stubs (see world_pathing.cpp).

#ifndef MATCH_PATHING_INCLUDED
#define MATCH_PATHING_INCLUDED

#include "world_terrain.h"
#include "entities/world_entity_common.h"

// Bit masks of terrain types, to build terrain checks from (see world_tile_move_cost).
static constexpr ui8 PATH_TERRAIN_LAND = (1 << (ui8)TERRAIN_TILE_TYPE::LAND_PLAINS) | (1 << (ui8)TERRAIN_TILE_TYPE::LAND_HILLS) | (1 << (ui8)TERRAIN_TILE_TYPE::LAND_MOUNTAINS);
static constexpr ui8 PATH_TERRAIN_WATER = (1 << (ui8)TERRAIN_TILE_TYPE::WATER_COAST) | (1 << (ui8)TERRAIN_TILE_TYPE::WATER_SEA);

// What a path may cross and how it travels. Zero = any terrain.
enum PATH_MODE : ui8
{
	PATH_MODE_ANY, // Any terrain.
	PATH_MODE_LAND,
	PATH_MODE_LAND_ROADS_ONLY, // Land, road tiles only.
	PATH_MODE_LAND_PREFER_ROADS, // Land, sticking to the road network where it helps.
	PATH_MODE_SEA,

	PATH_MODE_COUNT
};

// Request for a path. Zero-initialized members are the neutral default.
struct path_params
{
	world_location start;
	world_location end;
	PATH_MODE mode;
	entity_guid within_settlement_territory; // If set to a settlement entity, limits the search to the settlement's territory.
};

// Out structure for world_path_find: a computed path as a list of tiles, start excluded.
struct path
{
	world_location* points; // Tiles, in caller memory.
	ui16 points_capacity; // (Input) Size of the points memory, in tiles.
	ui16 count; // Number of tiles stored. 0 if start == end or no progress was possible.
	bool partial; // The path doesn't reach the end.
	ui32 cost; // Total cost of the path. 0 if partial or if start == end.
};

// Computes a path from params.start to params.end into out_path, whose points and points_capacity must be set by the caller, using the scratch arena for the search.
// Always outputs a path, possibly empty or partial. The scratch arena size is the effective limit of the search. It is not cleared by the function.
void world_path_find(const world_terrain& terrain, const path_params& params, mem_arena& scratch, path& out_path);

#endif // MATCH_PATHING_INCLUDED
