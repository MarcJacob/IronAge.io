// Pathfinding implementation: A* over the 8-neighbour tile grid, outputting paths as waypoints (one waypoint per change of direction).

// @NOTE(Marc): Most of this implementation was originall AI-generated. I've of course applied my usual policy of reviewing and manually changing stuff multiple times on this,
// but I've come to see that this will eventually need a rewrite, with three objectives:
// - Account for fractional tile location for more "straight line" movement instead of purely 8-directional.
// - Optimize movement over long distances, whether roads are followed or not (alternative "most direct" route mode)
// - Make movement look natural, which is helpful really since we don't care about exact mathematical correctness so long as the result is somewhat predictable and deterministic.
//
// What we have now works well enough.

#include "game_common/match/world/world_terrain.h"
#include "world_sim.h"
#include "world_movement.h"

// A* bookkeeping for one tile. Nodes live in an open-addressing hash table (linear probing) allocated from the scratch arena.
enum PATH_NODE_STATE : ui8
{
    PATH_NODE_EMPTY, // Unused slot (the arena is zeroed, so this must be 0).
    PATH_NODE_OPEN, // Discovered, waiting in the heap to be expanded.
    PATH_NODE_CLOSED, // Expanded: its g is final.
};

struct path_node
{
    ui32 tile; // World tile index, row major.
    ui32 g; // Cheapest known cost from the start.
    ui8 from_dir; // Index in NEIGHBOUR_OFFSETS_ALL of the step taken to reach this tile.
    PATH_NODE_STATE state;
};

// Open set entry.
struct path_heap_entry
{
    ui32 f; // Prospective new value for g if path goes through this tile. 
    ui32 g; // g cost value of tile this entry would be tested from.
    ui32 tile; // World tile index, row major.
};

struct path_search
{
    path_node* nodes;
    ui32 node_cap; // Maximum number of nodes in search buffer, set to the largest power of two lower or equal half of search buffer size.
    ui32 node_count; // Claimed slots. Capped at 3/4 of node_cap: linear probing gets very slow on a nearly full table.
    bool full; // Set once the table or the heap ran out of room. The search stops there and returns its best path so far.

    // Binary heap: an array where heap[i] is never worse than its children heap[2i+1] and heap[2i+2]. heap[0] is the best open entry.
    path_heap_entry* heap;
    ui32 heap_count, heap_cap;
};

// Tie-break order for entries of equal f: higher g (closer to the target when a.f == b.f) first, then lower tile index. Keeps results identical on every machine.
static bool heap_entry_better_than(const path_heap_entry& a, const path_heap_entry& b)
{
    if (a.f != b.f) return a.f < b.f;
    if (a.g != b.g) return a.g > b.g;
    return a.tile < b.tile;
}

// Returns false if the heap is full.
static bool path_heap_push(path_search& s, path_heap_entry entry)
{
    if (s.heap_count == s.heap_cap)
    {
        s.full = true;
        return false;
    }

    // Start at the "tip" of the heap.
    // Walk up from the new last slot, pulling down every parent that is worse than the entry.
    ui32 i = s.heap_count++;
    while (i > 0 && heap_entry_better_than(entry, s.heap[(i - 1) / 2]))
    {
        // Entry is better than parent: swap them.
        s.heap[i] = s.heap[(i - 1) / 2];
        i = (i - 1) / 2;
    }

    s.heap[i] = entry;
    return true;
}

// Removes and returns the best entry. The heap must not be empty.
static path_heap_entry path_heap_pop(path_search& s)
{
    path_heap_entry bestNode = s.heap[0];
    path_heap_entry finalElement = s.heap[s.heap_count - 1]; // Save the last element of the heap to be shifted back if needed.

    // Move the best binary branch up, with the best child becoming the parent recursively.
    ui32 i = 0;
    while (2 * i + 1 < s.heap_count)
    {
        // First child index
        ui32 bestChild = 2 * i + 1;

        // Does second child exist and have a better cost ?
        if (bestChild + 1 < s.heap_count && heap_entry_better_than(s.heap[bestChild + 1], s.heap[bestChild]))
            bestChild++;

        // The final element of the heap has to be moved somewhere else since we're reducing the heap's size.
        // To do so, we can just wait until we happen upon a spot in the tree that fits it, then break out early.
        if (!heap_entry_better_than(s.heap[bestChild], finalElement))
        {
            break;
        }

        // Overwrite parent (i) with child, then set new index to child's.
        s.heap[i] = s.heap[bestChild];
        i = bestChild;
    }

    // Place final element in the "hole" that's left in the heap, that being the last child to take its parent's place.
    s.heap[i] = finalElement;
    s.heap_count--;

    return bestNode;
}

// Node lookup for a given tile index in an active path search.
static path_node* path_node_get(path_search& search, ui32 tile)
{
    // Hash tile index.
    ui32 h = tile * 2654435761u;
    h ^= h >> 15;

    // If we're lucky the node we're looking for is right here, or the slot is empty and we can claim it.
    // If we're not lucky, we just start probing forward.
    for (ui32 probe = 0; probe < search.node_cap; probe++)
    {
        path_node& node = search.nodes[(h + probe) & (search.node_cap - 1)];
        if (node.state == PATH_NODE_EMPTY)
        {
            // Claiming a new slot. Refused past 3/4 load: probe chains get very long on a nearly full table.
            if (search.node_count >= search.node_cap / 4 * 3) break;
            search.node_count++;
            node.tile = tile;
            return &node;
        }
        if (node.tile == tile) return &node;
    }

    // The tile has no existing node in the hash map, and the map has no room left to add another one. The search must stop.
    search.full = true;
    return nullptr;
}

// Estimated cost left to the target: chamfer distance.
static ui32 path_heuristic(world_location from, world_location to)
{
    ui32 dx = ia_abs(to.x - from.x);
    ui32 dy = ia_abs(to.y - from.y);
    ui32 diagonal = dx < dy ? dx : dy;

    return (PROXIMITY_WEIGHT_ORTHOGONAL * (dx + dy - diagonal) + PROXIMITY_WEIGHT_DIAGONAL * diagonal);
}

// Long searches trade exact optimality for a more goal-directed search. Short searches
// retain ordinary A* behavior, while long searches spend less of the fixed node pool
// exploring equally plausible terrain around the start.
static constexpr ui32 PATH_WEIGHTED_HEURISTIC_DISTANCE = 32;
static constexpr ui32 PATH_WEIGHTED_HEURISTIC_WEIGHT = 3;

static ui32 path_heuristic_weight(world_location from, world_location to)
{
    ui32 dx = ia_abs(to.x - from.x);
    ui32 dy = ia_abs(to.y - from.y);
    return (dx > dy ? dx : dy) >= PATH_WEIGHTED_HEURISTIC_DISTANCE
        ? PATH_WEIGHTED_HEURISTIC_WEIGHT
        : 1;
}

void world_path_find(const world_terrain& terrain, world_location start, world_location end, const path_params& params,
    mem_arena& scratch, path& out_path)
{
    ASSERT(out_path.points != nullptr && out_path.points_capacity > 0);

    ui32 world_width = terrain.size_tiles.x;
    ui32 startTile = start.y * world_width + start.x, endTile = end.y * world_width + end.x;
    ui32 heuristicWeight = path_heuristic_weight(start, end);

    // Default result, kept if the scratch arena is too small to search at all.
    out_path.count = 0;
    out_path.length = 0;
    out_path.partial = true;

    // Scratch layout: a node table of at most half the arena (power of two), then a heap filling the rest.
    path_search search = {};
    ui64 nodeCap = 1;
    while (nodeCap * 2 * sizeof(path_node) <= (scratch.mem_size - scratch.allocated_count) / 2) nodeCap *= 2;
    if (nodeCap < 16) return;

    search.node_cap = (ui32)nodeCap;
    search.nodes = scratch.alloc<path_node>(nodeCap);

    search.heap_cap = (scratch.mem_size - scratch.allocated_count) / sizeof(path_heap_entry) - 1; // -1 for alignment padding.
    search.heap = scratch.alloc<path_heap_entry>(search.heap_cap, false);
    if (search.nodes == nullptr || search.heap == nullptr) return;

    path_node* startNode = path_node_get(search, startTile);
    startNode->state = PATH_NODE_OPEN;
    path_heap_push(search, { heuristicWeight * path_heuristic(start, end), 0, startTile });

    // Closest tile to the end expanded so far. Is the end itself unless it is unreachable or the search ran out of memory.
    ui32 bestTile = startTile, bestRemaining = ~0u;

    while (search.heap_count > 0 && !search.full)
    {
        path_heap_entry entry = path_heap_pop(search);
        path_node* node = path_node_get(search, entry.tile);
        if (node->state == PATH_NODE_CLOSED) continue; // Stale duplicate of an already expanded tile.

        if (path_heuristic({ (ui16)(entry.tile % world_width), (ui16)(entry.tile / world_width) }, end) < bestRemaining)
        {
            bestRemaining = path_heuristic({ (ui16)(entry.tile % world_width), (ui16)(entry.tile / world_width) }, end);
            bestTile = entry.tile;
        }
        if (entry.tile == endTile)
        {
            // Destination reached !
            node->state = PATH_NODE_CLOSED;
            break;
        }

        // Add non-closed neighbours or more expensive open neighbours to open heap.
        for (ui8 dir = 0; dir < 8; dir++)
        {
            i32 x = (i32)(entry.tile % world_width) + NEIGHBOUR_OFFSETS_ALL[dir].x;
            i32 y = (i32)(entry.tile / world_width) + NEIGHBOUR_OFFSETS_ALL[dir].y;

            if (x < 0 || y < 0 || x >= terrain.size_tiles.x || y >= terrain.size_tiles.y) continue;

            ui16 regionIndex, tileIndex;
            world_get_tile(terrain, { (ui16)x, (ui16)y }, regionIndex, tileIndex);

            ui8 cost = world_tile_move_cost(terrain, regionIndex, tileIndex, params);
            if (cost == 0) continue;

            bool isDiagonal = NEIGHBOUR_OFFSETS_ALL[dir].x != 0 && NEIGHBOUR_OFFSETS_ALL[dir].y != 0;
            ui32 g = entry.g + cost * (isDiagonal ? PROXIMITY_WEIGHT_DIAGONAL : PROXIMITY_WEIGHT_ORTHOGONAL);

            path_node* neighbour = path_node_get(search, y * world_width + x);
            if (neighbour == nullptr) continue; // Table full: this tile is simply never explored.

            // If the node was previously opened / explored, it will have a state value. Closed tiles are never re-tried, open nodes
            // can get a new heap entry if their new g cost would be lower than before.
            if (neighbour->state == PATH_NODE_CLOSED || (neighbour->state == PATH_NODE_OPEN && neighbour->g <= g)) continue;

            // Push new open node / existing open node with lower cost (in which case the older entry will be ignored when read as it
            // will be linked to a CLOSED node).
            if (!path_heap_push(search, path_heap_entry {
                        .f = g + heuristicWeight * path_heuristic({ (ui16)x, (ui16)y }, end),
                        .g = g, 
                        .tile = neighbour->tile 
            })) continue; // Heap full: same.

            neighbour->g = g;
            neighbour->from_dir = dir;
            neighbour->state = PATH_NODE_OPEN;
        }

        node->state = PATH_NODE_CLOSED;
    }

    // Walk back from the best tile to the start, twice. The path is found end first but waypoints are stored start first,
    // and only the first points_capacity of them are kept, so the first pass counts them and the second one writes them.
    // A tile is a waypoint if it is the last one, or if the step direction changes there. The start itself is not stored.
    out_path.partial = bestTile != endTile;
    ui16 total = 0;
    for (ui8 pass = 0; pass < 2; pass++)
    {
        ui16 seen = 0;
        ui8 laterDir = path_node_get(search, bestTile)->from_dir; // Direction of the step after the current tile.
        out_path.length = 0;

        for (ui32 tile = bestTile; tile != startTile; out_path.length++)
        {
            ui8 dir = path_node_get(search, tile)->from_dir;
            if (tile == bestTile || dir != laterDir)
            {
                ui16 index = total - 1 - seen++; // Only meaningful on the second pass.
                if (pass == 1 && index < out_path.points_capacity) out_path.points[index] = { (ui16)(tile % world_width), (ui16)(tile / world_width) };
            }

            laterDir = dir;
            tile = (ui32)((i32)tile - NEIGHBOUR_OFFSETS_ALL[dir].x - NEIGHBOUR_OFFSETS_ALL[dir].y * (i32)world_width);
        }
        total = seen;
    }
    out_path.count = total < out_path.points_capacity ? total : out_path.points_capacity;
}

world_path_store* world_path_store_init(mem_arena& memory)
{
    world_path_store* store = memory.alloc<world_path_store>();
    if (store == nullptr) return nullptr;

    ui8* scratchMemory = (ui8*)memory.alloc(WORLD_PATH_STORE_SCRATCH_SIZE);
    if (scratchMemory == nullptr) return nullptr;

    store->scratch = mem_arena_create(scratchMemory, WORLD_PATH_STORE_SCRATCH_SIZE);
    store->last.points = store->points;
    store->last.points_capacity = WORLD_PATH_STORE_MAX_WAYPOINTS;
    return store;
}

void world_request_path(match_world_state& world, world_location start, world_location end, const path_params& params, path_result& out_result)
{
    out_result = {};
    out_result.handle = INVALID_PATH_HANDLE; // The store only keeps the last path for now, so there is nothing to refer to later.

    // Nothing to do, or an end outside the world (which the search can't handle).
    if (start == end || end.x >= world.terrain->size_tiles.x || end.y >= world.terrain->size_tiles.y) return;

    // Each request overwrites the previous path.
    world_path_store& store = *world.path_store;
    store.scratch.clear();
    world_path_find(*world.terrain, start, end, params, store.scratch, store.last);

    out_result.count = store.last.count;
    out_result.length = store.last.length;
    out_result.partial = store.last.partial;
    if (store.last.count > 0) out_result.next_point = store.last.points[0];
}
