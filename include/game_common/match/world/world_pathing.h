// Public pathfinding symbols: path requests over the world's terrain, and helpers to follow the resulting paths.

#ifndef MATCH_PATHING_INCLUDED
#define MATCH_PATHING_INCLUDED

#include "world_terrain.h"
#include "entities/world_entity_common.h"

// Bit masks of terrain types a path may cross, to be combined into path_params::passable_terrain.
static constexpr ui8 PATH_TERRAIN_LAND = (1 << (ui8)TERRAIN_TILE_TYPE::LAND_PLAINS) | (1 << (ui8)TERRAIN_TILE_TYPE::LAND_HILLS) | (1 << (ui8)TERRAIN_TILE_TYPE::LAND_MOUNTAINS);
static constexpr ui8 PATH_TERRAIN_WATER = (1 << (ui8)TERRAIN_TILE_TYPE::WATER_COAST) | (1 << (ui8)TERRAIN_TILE_TYPE::WATER_SEA);

// Constraints a path must respect.
struct path_params
{
	ui8 passable_terrain; // Bit per TERRAIN_TILE_TYPE (see PATH_TERRAIN_* masks).

	bool roads_only : 1; // Only road tiles are passable.
	bool straight_segments : 1; // Waypoints joined by straight lines at any angle (line-of-sight merge), for movement. Off: waypoints at the direction changes of the 8-direction tile path (runs), for tile-bound uses (roads).
	bool require_complete : 1; // For tile-bound uses (roads): the path must reach the end, else the result is a failure (empty, not partial). The result is the same from either endpoint (see world_path_find).
    entity_guid within_settlement_territory; // If set to a settlement entity, will limit the search to the settlement's territory.
};

// In & Out structure for world_path_find. When processed, contains a computed path as a list of waypoints. 
// The start is not included: from the start, move in a straight line to points[0], then to points[1], and so on.
// With params.straight_segments, each leg is a straight line between tile centers at any angle, every tile it crosses being passable
// (the search runs on 8 directions, then a line-of-sight pass merges legs when the line costs no more than the 8-direction steps it replaces).
// Without it, each leg is a run of single steps in one of the 8 directions (see world_path_next_tile): the waypoints are the tiles where the direction changes, and the end.
struct path
{
	world_location* points; // Waypoints, in caller memory.
	ui16 points_capacity; // (Input) Maximum number of points we want, and the size of the points memory.
	ui16 count; // Number of waypoints stored. May be less than the path really has if the caller's capacity was smaller, and 0 if start == end or no progress was possible.
	ui16 length; // Total length of the whole path found, not only of the stored waypoints: sum over legs of max(dx, dy) in tiles (the step count of an 8-direction run).
	bool partial; // The path doesn't reach the target: it is unreachable or the search ran out of memory, and the path ends at the explored tile closest to it.
	ui32 cost; // Total cost of the whole path (tile move costs times chamfer weights, see PROXIMITY_WEIGHT_*), with merged legs at their line cost: never above the plain A* cost. 0 if partial or if start == end.
};

// Computes a path from start to end into out_path, whose points and points_capacity (above 0) must be set by the caller, using the scratch arena for the search.
// Always outputs a path, possibly empty or partial. The scratch arena size is the effective limit of the search.
// It is not cleared by the function, so the caller can reuse the memory afterwards.
//
// With params.straight_segments the waypoints are merged into straight lines, else they are the direction changes of the A* tile path.
// With params.require_complete, the search always runs from the lexicographically lower endpoint (lower x, then lower y) to the higher one, and the waypoints
// are reversed if the caller asked for the opposite direction, so both directions give the same route. A path that doesn't reach the end, or whose waypoints
// don't all fit in out_path.points_capacity, or whose start or end tile is impassable, is a failure: count 0, length 0, cost 0, partial false.
// The cost of a reversed path is recomputed for its own direction (start tile not charged, end tile charged).
void world_path_find(const world_terrain& terrain, world_location start, world_location end, const path_params& params,
	mem_arena& scratch, path& out_path);

// Walk over the tiles of one path segment, in order: the integer supercover of the straight line between the centers of its two end tiles, start tile excluded,
// end tile included. This is the exact traversal the pathfinder's line-of-sight merge charges costs on.
// A line passing exactly through a tile corner goes diagonally from one tile to the next: the two tiles that only touch
// the line at that point are not yielded. Deterministic, integer only. Create it with path_segment_walk_begin, never fill it by hand.
struct path_segment_walk
{
	world_location tile; // Last tile yielded by path_segment_walk_next (the segment start before the first call).
	i32 dx, dy; // Segment extent in tiles on each axis, absolute.
	i32 step_x, step_y; // Direction on each axis (+1 or -1).
	i32 crossed_x, crossed_y; // Tile borders crossed so far on each axis.
	i64 ex, ey; // max(dx, 1) and max(dy, 1).
	i64 total_time; // Time at the end center: 2 * ex * ey. Time 0 is the start center. Every tile border crossing falls on an integer time.
};

// Starts a walk from "from" towards "to". Yields nothing if they are the same tile.
path_segment_walk path_segment_walk_begin(world_location from, world_location to);

// Moves to the next tile of the segment, stored in walk.tile. Returns false once the end tile has been yielded (walk.tile is left unchanged).
// out_time is the time at which the line enters that tile (see total_time).
bool path_segment_walk_next(path_segment_walk& walk, i64& out_time);

// Writes the tiles of every leg of a computed path made of 8-direction runs (params.straight_segments off), the start tile itself excluded, to out_tiles.
// Stops at capacity. Returns the number of tiles written.
ui32 world_path_expand_tiles(world_location start, const path& p, world_location* out_tiles, ui32 capacity);

// Returns the tile following "from" on the way to "to": one step along each axis that is not yet aligned.
// Inside a run of a computed path (single direction, straight_segments off) this is exactly the path's next tile.
static inline world_location world_path_next_tile(world_location from, world_location to)
{
	world_location next = from;

	if (to.x > from.x) next.x++;
	else if (to.x < from.x) next.x--;

	if (to.y > from.y) next.y++;
	else if (to.y < from.y) next.y--;

	return next;
}

// Returns the next waypoint of a computed path to move straight towards from "from", or "from" itself if the path has no waypoint.
static inline world_location world_path_next_point(const path& p, world_location from)
{
	return p.count > 0 ? p.points[0] : from;
}

#endif // MATCH_PATHING_INCLUDED
