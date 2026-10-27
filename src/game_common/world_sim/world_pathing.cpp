// Pathfinding implementation: A* over the 8-neighbour tile grid, then either a line-of-sight merge (params.straight_segments: waypoints joined by straight lines) or waypoints at the direction changes (runs).

// @NOTE(Marc): Most of this implementation was originall AI-generated. I've of course applied my usual policy of reviewing and manually changing stuff multiple times on this,
// but I've come to see that this will eventually need a rewrite, with three objectives:
// - Account for fractional tile location for more "straight line" movement instead of purely 8-directional.
// - Optimize movement over long distances, whether roads are followed or not (alternative "most direct" route mode)
// - Make movement look natural, which is helpful really since we don't care about exact mathematical correctness so long as the result is somewhat predictable and deterministic.
//
// What we have now works well enough.

#include "game_common/match/world/world_state.h"
#include "game_common/match/world/world_terrain.h"
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

// Estimated cost left to the target: chamfer distance, assuming a tile cost of 1.
static ui32 path_heuristic(world_location from, world_location to)
{
    ui32 dx = ia_abs(to.x - from.x);
    ui32 dy = ia_abs(to.y - from.y);
    ui32 diagonal = dx < dy ? dx : dy;

    // Each diagonal step uses up one unit on both axes, so the orthogonal steps left are (dx - diagonal) + (dy - diagonal).
    return (TRAVEL_DIST_WEIGHT_ORTHOGONAL * (dx + dy - 2 * diagonal) + TRAVEL_DIST_WEIGHT_DIAGONAL * diagonal);
}

// Weighted A*: the heuristic is multiplied by this, for every search distance. Found paths cost at most this times the optimal.
// Must stay below the plains / road cost ratio (5: plains 25, road 5 in path cost units), or straight plains paths become as good as roads and roads get ignored.
// 2 keeps roads followed while open plains searches of ~100 tiles still fit the node budget.
static constexpr ui32 PATH_HEURISTIC_WEIGHT = 2;

// Longest line, in A* steps, the merge pass tries from one anchor. Bounds its cost (it is re-run on every path request).
static constexpr ui32 PATH_MERGE_MAX_STEPS = 64;

path_segment_walk path_segment_walk_begin(world_location from, world_location to)
{
    path_segment_walk walk = {};
    walk.tile = from;
    walk.dx = ia_abs((i32)to.x - from.x);
    walk.dy = ia_abs((i32)to.y - from.y);
    walk.step_x = to.x > from.x ? 1 : -1;
    walk.step_y = to.y > from.y ? 1 : -1;
    walk.ex = walk.dx > 0 ? walk.dx : 1;
    walk.ey = walk.dy > 0 ? walk.dy : 1;
    walk.total_time = 2 * walk.ex * walk.ey;
    return walk;
}

bool path_segment_walk_next(path_segment_walk& walk, i64& out_time)
{
    if (walk.crossed_x >= walk.dx && walk.crossed_y >= walk.dy) return false;

    // Time of the next border crossing on each axis, "never" once an axis is done.
    i64 nextX = walk.crossed_x < walk.dx ? (2 * walk.crossed_x + 1) * walk.ey : walk.total_time;
    i64 nextY = walk.crossed_y < walk.dy ? (2 * walk.crossed_y + 1) * walk.ex : walk.total_time;
    out_time = nextX < nextY ? nextX : nextY;

    // Crossing both borders at the same time means passing exactly through a corner: the step is diagonal.
    if (nextX <= nextY) { walk.crossed_x++; walk.tile.x = (ui16)(walk.tile.x + walk.step_x); }
    if (nextY <= nextX) { walk.crossed_y++; walk.tile.y = (ui16)(walk.tile.y + walk.step_y); }
    return true;
}

ui32 world_path_expand_tiles(world_location start, const path& p, world_location* out_tiles, ui32 capacity)
{
    ui32 written = 0;
    world_location tile = start;
    for (ui16 i = 0; i < p.count; i++)
    {
        // Each leg is a run in one direction: world_path_next_tile steps along it.
        while (tile != p.points[i])
        {
            if (written == capacity) return written;
            tile = world_path_next_tile(tile, p.points[i]);
            out_tiles[written++] = tile;
        }
    }
    return written;
}

// Cost of the straight line between the centers of two distinct tiles, in A* units (tile cost x chamfer length: 5 per orthogonal tile, 7 per diagonal one).
// Returns false if any tile the line crosses is impassable for params. The start tile is not checked, like in A*.
//
// Tiles are the integer supercover of the line: all tiles it goes through. A line passing exactly through a tile corner goes diagonally from one tile to the
// next, the two tiles that only touch the line at that point do not count (the same diagonal move A* allows).
// Time t runs from 0 (start center) to T (end center), T = 2 * ex * ey, with ex = max(dx, 1) and ey = max(dy, 1), so that every tile border crossing
// falls on an integer: the k-th x border is crossed at (2k - 1) * ey, the k-th y border at (2k - 1) * ex.
//
// Cost = W * sum(tile cost x share of the line) with W = 5 * max(dx, dy) + 2 * min(dx, dy) (chamfer length of the whole line).
// A tile's share runs from the middle of the previous tile's chord (the part of the line inside the tile) to the middle of its own, the start counting as
// the middle of the first chord and the end as the middle of the last one. The shares sum to the whole line. This is how A* charges a step (center to center
// at the cost of the tile entered), so a line along an existing 8-direction run costs exactly what A* charged for it, and a shortcut over cheaper or
// costlier terrain than the run costs less or more. Integer math: shares are in doubled time units, divided once at the end, rounded to nearest.
static bool path_line_cost(const world_terrain& terrain, const path_params& params, world_location from, world_location to, ui32& out_cost)
{
    path_segment_walk walk = path_segment_walk_begin(from, to);
    i64 totalTime = walk.total_time;

    i64 chordStart = 0; // Time at which the line entered the current tile.
    i64 previousMiddle = 0; // Doubled time at the middle of the previous tile's chord.
    i64 weighted = 0; // Sum of tile cost x doubled time share, for the tiles left so far (start excluded).
    ui8 tileCost = 0; // Cost of the current tile. 0 for the start tile, which is not charged.

    i64 t;
    while (path_segment_walk_next(walk, t))
    {
        // Leaving the previous tile: its chord is [chordStart, t].
        i64 middle = chordStart + t; // Doubled.
        if (tileCost > 0) weighted += (i64)tileCost * (middle - previousMiddle);
        previousMiddle = tileCost > 0 ? middle : 0;
        chordStart = t;

        ui16 regionIndex, tileIndex;
        world_get_tile(terrain, walk.tile, regionIndex, tileIndex);
        tileCost = world_tile_move_cost(terrain, regionIndex, tileIndex, params);
        if (tileCost == 0) return false;
    }

    // The last tile ends at the end of the line, its chord middle being the end.
    weighted += (i64)tileCost * (2 * totalTime - previousMiddle);

    i64 dx = walk.dx, dy = walk.dy;
    i64 chamferLength = 5 * (dx > dy ? dx : dy) + 2 * (dx < dy ? dx : dy);
    out_cost = (ui32)((weighted * chamferLength + totalTime) / (2 * totalTime));
    return true;
}

// The search itself, from start to end as given (see world_path_find for the public behavior).
static void path_search_run(const world_terrain& terrain, world_location start, world_location end, const path_params& params,
    mem_arena& scratch, path& out_path)
{
    ASSERT(out_path.points != nullptr && out_path.points_capacity > 0);

    ui32 world_width = terrain.size_tiles.x;
    ui32 startTile = start.y * world_width + start.x, endTile = end.y * world_width + end.x;

    // Default result, kept if the scratch arena is too small to search at all.
    out_path.count = 0;
    out_path.length = 0;
    out_path.partial = true;
    out_path.cost = 0;

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
    path_heap_push(search, { PATH_HEURISTIC_WEIGHT * path_heuristic(start, end), 0, startTile });

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
            ui32 g = entry.g + cost * (isDiagonal ? TRAVEL_DIST_WEIGHT_DIAGONAL : TRAVEL_DIST_WEIGHT_ORTHOGONAL);

            path_node* neighbour = path_node_get(search, y * world_width + x);
            if (neighbour == nullptr) continue; // Table full: this tile is simply never explored.

            // If the node was previously opened / explored, it will have a state value. Closed tiles are never re-tried, open nodes
            // can get a new heap entry if their new g cost would be lower than before.
            if (neighbour->state == PATH_NODE_CLOSED || (neighbour->state == PATH_NODE_OPEN && neighbour->g <= g)) continue;

            // Push new open node / existing open node with lower cost (in which case the older entry will be ignored when read as it
            // will be linked to a CLOSED node).
            if (!path_heap_push(search, path_heap_entry {
                        .f = g + PATH_HEURISTIC_WEIGHT * path_heuristic({ (ui16)x, (ui16)y }, end),
                        .g = g, 
                        .tile = neighbour->tile 
            })) continue; // Heap full: same.

            neighbour->g = g;
            neighbour->from_dir = dir;
            neighbour->state = PATH_NODE_OPEN;
        }

        node->state = PATH_NODE_CLOSED;
    }

    // Walk back from the best tile to the start, twice: the first pass counts the steps, the second one lays the tile path out start first.
    // The search heap is dead by now, so its entries are reused as the tile path: .tile = tile index, .g = cost from the start. Index 0 is the start.
    // The path has at most node_count tiles, which is far below heap_cap (the node table takes at most half of the arena, the heap the rest at 12 bytes an entry).
    ui32 steps = 0;
    for (ui32 tile = bestTile; tile != startTile; steps++)
    {
        ui8 dir = path_node_get(search, tile)->from_dir;
        tile = (ui32)((i32)tile - NEIGHBOUR_OFFSETS_ALL[dir].x - NEIGHBOUR_OFFSETS_ALL[dir].y * (i32)world_width);
    }
    ASSERT(steps < search.heap_cap);

    for (ui32 tile = bestTile, i = steps; ; i--)
    {
        path_node* node = path_node_get(search, tile);
        search.heap[i].tile = tile;
        search.heap[i].g = node->g;
        if (i == 0) break;

        tile = (ui32)((i32)tile - NEIGHBOUR_OFFSETS_ALL[node->from_dir].x - NEIGHBOUR_OFFSETS_ALL[node->from_dir].y * (i32)world_width);
    }

    out_path.partial = bestTile != endTile;
    ui32 mergedCost = 0;
    ui16 total = 0;
    out_path.length = 0;

    if (!params.straight_segments)
    {
        // Runs: a tile is a waypoint if it is the last one, or if the step direction changes there (the step into it differs from the step out of it).
        // Steps are compared as tile index differences, which are unique per direction. Waypoints are stored start first, only the first points_capacity are kept.
        for (ui32 i = 1; i <= steps; i++)
        {
            if (i < steps && (i32)search.heap[i + 1].tile - (i32)search.heap[i].tile == (i32)search.heap[i].tile - (i32)search.heap[i - 1].tile) continue;

            if (total < out_path.points_capacity) out_path.points[total] = { (ui16)(search.heap[i].tile % world_width), (ui16)(search.heap[i].tile / world_width) };
            total++;
        }
        out_path.length = (ui16)steps;
        mergedCost = search.heap[steps].g;
    }

    // Line-of-sight merge: from an anchor tile, extend a straight line to the farthest later tile of the path (scanning forward, stopping at the first
    // refusal) such that the line is passable and costs no more than the A* subpath it replaces. The end of that line is the next waypoint and anchor.
    // Waypoints are stored start first, only the first points_capacity are kept. The start itself is not stored.
    for (ui32 anchor = 0; params.straight_segments && anchor < steps; )
    {
        world_location anchorLoc = { (ui16)(search.heap[anchor].tile % world_width), (ui16)(search.heap[anchor].tile / world_width) };

        // The next tile is always reachable, at A*'s own cost.
        ui32 reach = anchor + 1;
        ui32 reachCost = search.heap[reach].g - search.heap[anchor].g;

        for (ui32 j = anchor + 2; j <= steps && j <= anchor + PATH_MERGE_MAX_STEPS; j++)
        {
            world_location jLoc = { (ui16)(search.heap[j].tile % world_width), (ui16)(search.heap[j].tile / world_width) };
            ui32 lineCost;
            if (!path_line_cost(terrain, params, anchorLoc, jLoc, lineCost) || lineCost > search.heap[j].g - search.heap[anchor].g) break;

            reach = j;
            reachCost = lineCost;
        }

        world_location reachLoc = { (ui16)(search.heap[reach].tile % world_width), (ui16)(search.heap[reach].tile / world_width) };
        if (total < out_path.points_capacity) out_path.points[total] = reachLoc;
        total++;

        mergedCost += reachCost;
        out_path.length += (ui16)ia_max(ia_abs((i32)reachLoc.x - anchorLoc.x), ia_abs((i32)reachLoc.y - anchorLoc.y));
        anchor = reach;
    }

    out_path.cost = out_path.partial ? 0 : mergedCost;
    out_path.count = total < out_path.points_capacity ? total : out_path.points_capacity;
}

void world_path_find(const world_terrain& terrain, world_location start, world_location end, const path_params& params,
    mem_arena& scratch, path& out_path)
{
    if (!params.require_complete)
    {
        path_search_run(terrain, start, end, params, scratch, out_path);
        return;
    }

    // Complete paths only, searched from the lexicographically lower endpoint so that A to B and B to A are the same route.
    bool reversed = start.x > end.x || (start.x == end.x && start.y > end.y);
    world_location from = reversed ? end : start, to = reversed ? start : end;

    // Both endpoints must be passable (A* never checks its start), so that a path exists from A to B exactly when it exists from B to A.
    ui16 regionIndex, tileIndex;
    world_get_tile(terrain, start, regionIndex, tileIndex);
    bool startPassable = world_tile_move_cost(terrain, regionIndex, tileIndex, params) > 0;

    if (startPassable) path_search_run(terrain, from, to, params, scratch, out_path);

    // Complete = the last stored waypoint is the end (a path never visits a tile twice, so a truncated waypoint list cannot end on it).
    bool complete = startPassable && (from == to || (!out_path.partial && out_path.count > 0 && out_path.points[out_path.count - 1] == to));
    if (!complete)
    {
        out_path.count = 0;
        out_path.length = 0;
        out_path.partial = false;
        out_path.cost = 0;
        return;
    }

    if (!reversed || out_path.count == 0) return;

    // Waypoints from the lower endpoint: p1 ... pk (pk = higher endpoint = the requested start). Asked from the higher one they are
    // pk-1 ... p1, then the lower endpoint. Reversing p1..pk-1 in place and overwriting pk does it.
    ui16 count = out_path.count;
    for (ui16 i = 0; i < (count - 1) / 2; i++)
    {
        world_location swap = out_path.points[i];
        out_path.points[i] = out_path.points[count - 2 - i];
        out_path.points[count - 2 - i] = swap;
    }
    out_path.points[count - 1] = from;

    // The cost charges the tiles entered, so it changes with the direction: recompute it leg by leg along the reversed waypoints (the length does not change).
    ui32 cost = 0;
    world_location legStart = start;
    for (ui16 i = 0; i < count; i++)
    {
        ui32 legCost;
        if (!path_line_cost(terrain, params, legStart, out_path.points[i], legCost))
        {
            out_path.count = 0;
            out_path.length = 0;
            out_path.cost = 0;
            return;
        }
        cost += legCost;
        legStart = out_path.points[i];
    }
    out_path.cost = cost;
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
    out_result.cost = store.last.cost;
    if (store.last.count > 0) out_result.next_point = store.last.points[0];
}
