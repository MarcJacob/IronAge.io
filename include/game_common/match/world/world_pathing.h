// Public pathfinding symbols: path requests over the world's terrain, and helpers to follow the resulting paths.

#ifndef MATCH_PATHING_INCLUDED
#define MATCH_PATHING_INCLUDED

#include "world_terrain.h"
#include "world_entities.h"

// Bit masks of terrain types a path may cross, to be combined into path_params::passable_terrain.
static constexpr ui8 PATH_TERRAIN_LAND = (1 << (ui8)TERRAIN_TILE_TYPE::LAND_PLAINS) | (1 << (ui8)TERRAIN_TILE_TYPE::LAND_HILLS) | (1 << (ui8)TERRAIN_TILE_TYPE::LAND_MOUNTAINS);
static constexpr ui8 PATH_TERRAIN_WATER = (1 << (ui8)TERRAIN_TILE_TYPE::WATER_COAST) | (1 << (ui8)TERRAIN_TILE_TYPE::WATER_SEA);

// Constraints a path must respect.
struct path_params
{
	ui8 passable_terrain; // Bit per TERRAIN_TILE_TYPE (see PATH_TERRAIN_* masks).

	bool roads_only : 1; // Only road tiles are passable.
    entity_guid within_settlement_territory; // If set to a settlement entity, will limit the search to the settlement's territory.
};

// In & Out structure for world_path_find. When processed, contains a computed path as a list of waypoints. 
// The start is not included: from the start, move in a straight line (see world_path_next_tile)
// to points[0], then to points[1], and so on. Each leg is a run of single steps in one of the 8 directions.
// TODO(Marc): Upgrade related algorithm so that we could store any "straight" line instead of only the 8 cardinal directions which should massively cut down on the
// number of segments in some specific circumstances, or allow more efficient paths.
struct path
{
	world_location* points; // Waypoints, in caller memory.
	ui16 points_capacity; // (Input) Maximum number of points we want, and the size of the points memory.
	ui16 count; // Number of waypoints stored. May be less than the path really has if the caller's capacity was smaller, and 0 if start == end or no progress was possible.
	ui16 length; // Total length in tiles of the whole path found, not only of the stored waypoints.
	bool partial; // The path doesn't reach the target: it is unreachable or the search ran out of memory, and the path ends at the explored tile closest to it.
	ui32 cost; // Total cost of the whole path (tile move costs times chamfer weights, see PROXIMITY_WEIGHT_*). 0 if partial or if start == end.
};

// Computes a path from start to end into out_path, whose points and points_capacity (above 0) must be set by the caller, using the scratch arena for the search.
// Always outputs a path, possibly empty or partial. The scratch arena size is the effective limit of the search.
// It is not cleared by the function, so the caller can reuse the memory afterwards.
void world_path_find(const world_terrain& terrain, world_location start, world_location end, const path_params& params,
	mem_arena& scratch, path& out_path);

// Returns the tile following "from" on the way to "to": one step along each axis that is not yet aligned.
// Inside a run of a computed path (single direction) this is exactly the path's next tile.
static inline world_location world_path_next_tile(world_location from, world_location to)
{
	world_location next = from;

	if (to.x > from.x) next.x++;
	else if (to.x < from.x) next.x--;

	if (to.y > from.y) next.y++;
	else if (to.y < from.y) next.y--;

	return next;
}

#endif // MATCH_PATHING_INCLUDED
