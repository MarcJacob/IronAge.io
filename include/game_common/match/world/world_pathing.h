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
    // Essential parameters.
	world_location start;
	world_location end;
	PATH_MODE mode;

    // Optional.
	entity_guid opt_settlement_territory; // If set to a settlement entity, limits the search to the settlement's territory.
    // ...
};

// Out structure for world_path_find: a computed path as a list of segments delimited by successive world locations.
// Also how paths are stored inside the world path store.
struct path
{
	world_location* points; // Path segment tails. First location is to be reached from wherever the traveller is right now.
	ui16 point_count; // (Input) Size of the points memory, in tiles.

	ui16 length; // Number of traversed tiles.
	ui32 cost; // Total travel cost of the path.
    
    inline world_location get_start() const { return points[0]; }
    inline world_location get_end() const { return points[point_count - 1]; }
};

// Computes a path from params.start to params.end into out_path, whose points and points_capacity must be set by the caller, using the scratch arena for the search.
// Always outputs a path, possibly empty or partial. The scratch arena size is the effective limit of the search. It is not cleared by the function.
// Does not necessarily return the best possible path, but rather the path an entity would take.
void world_path_find(const match_world_state& world, const path_params& params, mem_arena& scratch, path& out_path);

// Internal acceleration structure used by the world, defining a graph of pathing zones / components to quickly
// determine reachability and facilitate the pathfinding process, and cache commonly requested paths / itineraries.
struct world_path_store
{
    // Handle to a stored path.
    union path_handle
    {
        ui32 value;
        struct
        {
            ui16 index; // Index of path in internal storage array.
            ui16 creation_tick; // modulated tick this path was created on.
        };
    };

    mem_arena search_scratch; // Search memory.
    mem_arena storage; // Storage memory for cached paths, and the world travel acceleration structure.
    
    inline bool is_valid_handle(const path_handle& handle) const { return false; } // @TODO(Marc): Lookup path storage.
};

// Identifies a path kept by the world's path store. 0 = no path.
static constexpr world_path_store::path_handle INVALID_PATH_HANDLE {0};

#endif // MATCH_PATHING_INCLUDED
