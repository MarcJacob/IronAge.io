// Main collection of symbols for accessing / running the world simulation systems.

#ifndef WORLD_SIM_INCLUDED
#define WORLD_SIM_INCLUDED

#include "game_common/match/start_params.h"
#include "game_common/match/world.h"

// ENTITIES

// Spawns a new settlement in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_settlement(match_world_state& world, const world_entity_settlements::single& start_state);

// Spawns a new caravan in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_caravan(match_world_state& world, const world_entity_caravans::single& start_state);

// Spawns a new army in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_army(match_world_state& world, const world_entity_armies::single& start_state);

void world_destroy_settlement(match_world_state& world, entity_guid settlement);
void world_destroy_caravan(match_world_state& world, entity_guid caravan);
void world_destroy_army(match_world_state& world, entity_guid army);

void world_settlement_decrease_population(match_world_state& world, entity_guid settlement, ui32 amount);

bool world_alloc_settlements(match_world_state& world, mem_arena& memory, ui32 count);

bool world_alloc_caravans(match_world_state& world, mem_arena& memory, ui32 count);

bool world_alloc_armies(match_world_state& world, mem_arena& memory, ui32 count);

// SYSTEMS & HELPERS

// Claims a tile in the world for the given settlement using the origin proximity value as proximity base.
// Ensure settlement's influence bounds rectangle is kept up to date !
ui16 world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index);

// PATHFINDING
// A* over world tiles, deterministic for a given world state (ties broken by f, h, then node order).
// Paths are stored in a fixed pool and referred to by handle. Any path can be evicted at any time (oldest first, to make room), after which its handle is stale:
// owners must check with path_get before use and recompute when it fails.

// Rules for a path search.
struct path_params
{
    bool maritime; // True = only water tiles are walkable, false = only land tiles.
    bool allow_wilderness; // Whether unowned tiles are walkable.
    ui32 wilderness_extra_cost; // Added to the travel cost of each unowned tile.
    entity_guid only_settlement; // If valid, owned tiles must belong to this settlement.
    match_player_id only_player; // If not INVALID_MATCH_PLAYER_ID, owned tiles must belong to a settlement of this player. Both restrictions apply if both are set.
    ui32 max_nodes; // Upper bound on explored tiles, further capped by the search buffer's capacity.
};

enum PATH_STATUS : ui8
{
    PATH_COMPLETE, // Reaches the target.
    PATH_PARTIAL, // Search ran out of nodes: ends at the explored tile closest to the target.
};

// Reference to a stored path. generation 0 never matches a live path, so a zeroed handle is invalid.
struct path_handle
{
    ui16 slot;
    ui16 generation;
};

// Read-only view of a stored path, valid until the next path_find or path_free call.
struct path_view
{
    PATH_STATUS status;
    ui32 cost; // Total travel cost along the path, wilderness extra included.
    ui32 tile_count;
    const world_location* tiles; // In order from a start tile to the end tile.
};

// Storage for the paths pool and the A* search buffers. One per world (see match_world_state::paths), allocated and initialized at world init.
// Sizes are @PLACEHOLDER(tune), the search buffers are reused by every path_find call.
struct path_search_node
{
    world_location location;
    ui32 g; // Cost from the closest start tile.
    ui32 h; // Estimated remaining cost to the target, never overestimated.
    ui32 parent; // Node index of the previous tile on the path, ~0 for start tiles.
    ui32 heap_pos; // Position in the open heap, ~0 if not in it.
};

struct world_path_store
{
    static constexpr ui32 SLOT_COUNT = 64;
    static constexpr ui32 SLOT_TILE_CAPACITY = 256; // Longest storable path, longer ones are cut and marked partial.
    static constexpr ui32 SEARCH_NODE_CAPACITY = 4096;
    static constexpr ui32 SEARCH_HASH_SIZE = 8192; // Power of 2, at least twice the node capacity.

    // Bumped whenever the world may have changed in a way that alters path results (every applied command, every world tick).
    // A stored path is only reused by path_find for the same request in the same epoch, where recomputing would give the same path.
    ui64 epoch;

    // Stored paths.
    ui16 generations[SLOT_COUNT]; // Never 0.
    // The request each path answers, for reuse.
    ui64 epochs[SLOT_COUNT];
    path_params requests[SLOT_COUNT];
    world_location request_targets[SLOT_COUNT];
    ui32 request_start_counts[SLOT_COUNT]; // ~0 if the starts did not fit in the slot, in which case the path is never reused.
    world_location request_starts[SLOT_COUNT * SLOT_TILE_CAPACITY];
    ui64 sequences[SLOT_COUNT]; // Allocation order of the path in each slot, 0 if the slot is free. Oldest is evicted first.
    ui64 next_sequence;
    PATH_STATUS statuses[SLOT_COUNT];
    ui32 costs[SLOT_COUNT];
    ui32 tile_counts[SLOT_COUNT];
    world_location tiles[SLOT_COUNT * SLOT_TILE_CAPACITY];

    // Search buffers.
    path_search_node nodes[SEARCH_NODE_CAPACITY];
    ui32 node_count;
    ui32 heap[SEARCH_NODE_CAPACITY]; // Open set as a binary min-heap of node indices.
    ui32 heap_count;
    ui32 hash[SEARCH_HASH_SIZE]; // Open addressing table from tile location to node index + 1, 0 if empty.
};

// Prepares a zeroed path store for use.
void world_path_store_init(world_path_store& store);

// Searches a path from the closest of the given start tiles to the target, and stores it.
// If a path for the exact same request was already stored in the current epoch, its handle is returned instead of searching again.
// Returns a handle to the stored path, or an invalid one if no tile could be reached at all or the start tiles are not walkable.
// The world is const as only the path store is modified, which is a cache: handles are NOT the same across machines, only the paths they refer to are.
path_handle path_find(const match_world_state& world, const world_location* starts, ui32 start_count, world_location target, const path_params& params);

// Fills out_view and returns true if the handle is still live, false if it is stale or invalid.
bool path_get(const match_world_state& world, path_handle handle, path_view& out_view);

// Frees the stored path. Does nothing if the handle is stale.
void path_free(match_world_state& world, path_handle handle);

void world_system_economy_proximity_recalc(match_world_state& world);
void world_system_economy_influence_spread(match_world_state& world);

void world_system_economy_contestation(match_world_state& world);

void world_system_economy_update_pressure(match_world_state& world);

void world_system_economy_pop_growth(match_world_state& world);

void world_system_settlement_spawn_caravans(match_world_state& world);

void world_system_entity_movement_decision(match_world_state& world);

void world_system_entity_movement(match_world_state& world);

void world_system_caravan_arrivals(match_world_state& world);

void world_system_armies_target_reached(match_world_state& world);

// LIFECYCLE

// Initializes a new world state into the given memory from match start parameters.
match_world_state* world_state_init(mem_arena& memory, const game_match_start_params& start_params);

// Advances the world state by one tick.
void world_state_tick(match_world_state& world);

#endif // WORLD_SIM_INCLUDED
