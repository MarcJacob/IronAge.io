// Pathfinding implementation: A* over the 8-neighbour tile grid, outputting paths as waypoints (one waypoint per change of direction).

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
    ui32 f; // New cost if path goes through this tile.
    ui32 g; // Total accumulated path cost before the tile.
    ui32 tile; // World tile index, row major.
};

struct path_search
{
    path_node* nodes;
    ui32 node_cap; // Power of two.
    ui32 node_count; // Claimed slots. Capped at 3/4 of node_cap: linear probing gets very slow on a nearly full table.
    bool full; // Set once the table or the heap ran out of room. The search stops there and returns its best path so far.

    // Binary heap: an array where heap[i] is never worse than its children heap[2i+1] and heap[2i+2]. heap[0] is the best open entry.
    path_heap_entry* heap;
    ui32 heap_count, heap_cap;
};

// Tie-break order for entries of equal f: higher g (closer to the target) first, then lower tile index. Keeps results identical on every machine.
static bool path_heap_before(const path_heap_entry& a, const path_heap_entry& b)
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

    // Walk up from the new last slot, pulling down every parent that is worse than the entry.
    ui32 i = s.heap_count++;
    while (i > 0 && path_heap_before(entry, s.heap[(i - 1) / 2]))
    {
        s.heap[i] = s.heap[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    s.heap[i] = entry;
    return true;
}

// Removes and returns the best entry. The heap must not be empty.
static path_heap_entry path_heap_pop(path_search& s)
{
    path_heap_entry best = s.heap[0];
    path_heap_entry last = s.heap[--s.heap_count];

    // Walk down from the root, pulling up the better child until the last entry fits.
    ui32 i = 0;
    while (2 * i + 1 < s.heap_count)
    {
        ui32 child = 2 * i + 1;
        if (child + 1 < s.heap_count && path_heap_before(s.heap[child + 1], s.heap[child])) child++;
        if (!path_heap_before(s.heap[child], last)) break;

        s.heap[i] = s.heap[child];
        i = child;
    }
    s.heap[i] = last;
    return best;
}

// Node lookup for a given tile index in an active path search.
static path_node* path_node_get(path_search& s, ui32 tile)
{
    // Hash tile index.
    ui32 h = tile * 2654435761u;
    h ^= h >> 15;

    // If we're lucky the node we're looking for is right here, or the slot is empty and we can claim it.
    // If we're not lucky, we just start probing forward.
    for (ui32 probe = 0; probe < s.node_cap; probe++)
    {
        path_node& node = s.nodes[(h + probe) & (s.node_cap - 1)];
        if (node.state == PATH_NODE_EMPTY)
        {
            // Claiming a new slot. Refused past 3/4 load: probe chains get very long on a nearly full table.
            if (s.node_count >= s.node_cap / 4 * 3) break;
            s.node_count++;
            node.tile = tile;
            return &node;
        }
        if (node.tile == tile) return &node;
    }

    // The tile has no existing node in the hash map, and the map has no room left to add another one. The search must stop.
    s.full = true;
    return nullptr;
}

// Cheapest land terrain cost (LAND_PLAINS..LAND_MOUNTAINS are the first entries of the table). Searches are land-only.
static constexpr ui32 min_land_cost(ui32 a, ui32 b, ui32 c) { return a < b ? (a < c ? a : c) : (b < c ? b : c); }
static constexpr ui32 HEURISTIC_TILE_COST = min_land_cost(TILE_TERRAIN_TRAVEL_COST[(ui8)TERRAIN_TILE_TYPE::LAND_PLAINS],
    TILE_TERRAIN_TRAVEL_COST[(ui8)TERRAIN_TILE_TYPE::LAND_HILLS], TILE_TERRAIN_TRAVEL_COST[(ui8)TERRAIN_TILE_TYPE::LAND_MOUNTAINS]);

// Estimated cost left to the target: chamfer distance, assuming every tile costs the cheapest ordinary (non-road) terrain.
// Deliberately NOT admissible on roads (cost 1 < HEURISTIC_TILE_COST): paths across roads may be marginally suboptimal,
// in exchange for far fewer expanded nodes. Shared by sim and client preview, so both stay deterministic.
static ui32 path_heuristic(world_location from, world_location to)
{
    ui32 dx = from.x > to.x ? from.x - to.x : to.x - from.x;
    ui32 dy = from.y > to.y ? from.y - to.y : to.y - from.y;
    ui32 diagonal = dx < dy ? dx : dy;

    return HEURISTIC_TILE_COST * (PROXIMITY_WEIGHT_ORTHOGONAL * (dx + dy - diagonal) + PROXIMITY_WEIGHT_DIAGONAL * diagonal);
}

void world_path_find(const world_terrain& terrain, world_location start, world_location end, const path_params& params,
    mem_arena& scratch, path& out_path)
{
    ASSERT(out_path.points != nullptr && out_path.points_capacity > 0);

    ui32 world_width = terrain.size_tiles.x;
    ui32 startTile = start.y * world_width + start.x, endTile = end.y * world_width + end.x;

    // Default result, kept if the scratch arena is too small to search at all.
    out_path.count = 0;
    out_path.length = 0;
    out_path.partial = true;

    // Scratch layout: a node table of at most half the arena (power of two), then a heap filling the rest.
    path_search s = {};
    ui64 nodeCap = 1;
    while (nodeCap * 2 * sizeof(path_node) <= (scratch.mem_size - scratch.allocated_count) / 2) nodeCap *= 2;
    if (nodeCap < 16) return;

    s.node_cap = (ui32)nodeCap;
    s.nodes = scratch.alloc<path_node>(nodeCap);
    s.heap_cap = (scratch.mem_size - scratch.allocated_count) / sizeof(path_heap_entry) - 1; // -1 for alignment padding.
    s.heap = scratch.alloc<path_heap_entry>(s.heap_cap, false);
    if (s.nodes == nullptr || s.heap == nullptr) return;

    path_node* startNode = path_node_get(s, startTile);
    startNode->state = PATH_NODE_OPEN;
    path_heap_push(s, { path_heuristic(start, end), 0, startTile });

    // Closest tile to the end expanded so far. Is the end itself unless it is unreachable or the search ran out of memory.
    ui32 bestTile = startTile, bestRemaining = ~0u;

    while (s.heap_count > 0 && !s.full)
    {
        path_heap_entry entry = path_heap_pop(s);
        path_node* node = path_node_get(s, entry.tile);
        if (node->state == PATH_NODE_CLOSED) continue; // Stale duplicate of an already expanded tile.
        node->state = PATH_NODE_CLOSED;

        if (entry.f - entry.g < bestRemaining) { bestRemaining = entry.f - entry.g; bestTile = entry.tile; }
        if (entry.tile == endTile) break;

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

            path_node* neighbour = path_node_get(s, y * world_width + x);
            if (neighbour == nullptr) continue; // Table full: this tile is simply never explored.
            if (neighbour->state == PATH_NODE_CLOSED || (neighbour->state == PATH_NODE_OPEN && neighbour->g <= g)) continue;

            // Better route to the neighbour. Its older heap entry (if any) is left behind and skipped when popped.
            if (!path_heap_push(s, { g + path_heuristic({ (ui16)x, (ui16)y }, end), g, neighbour->tile })) continue; // Heap full: same.
            neighbour->g = g;
            neighbour->from_dir = dir;
            neighbour->state = PATH_NODE_OPEN;
        }
    }

    // Walk back from the best tile to the start, twice. The path is found end first but waypoints are stored start first,
    // and only the first points_capacity of them are kept, so the first pass counts them and the second one writes them.
    // A tile is a waypoint if it is the last one, or if the step direction changes there. The start itself is not stored.
    out_path.partial = bestTile != endTile;
    ui16 total = 0;
    for (ui8 pass = 0; pass < 2; pass++)
    {
        ui16 seen = 0;
        ui8 laterDir = path_node_get(s, bestTile)->from_dir; // Direction of the step after the current tile.
        out_path.length = 0;

        for (ui32 tile = bestTile; tile != startTile; out_path.length++)
        {
            ui8 dir = path_node_get(s, tile)->from_dir;
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
