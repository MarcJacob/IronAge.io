// Implementation file for tile pathfinding in the world sim (A*) and the pool of stored paths.

#include "world_sim.h"
#include "world_tile_travel.h"

static constexpr ui32 PATH_NONE = ~0u;

void world_path_store_init(world_path_store& store)
{
    for (ui32 slot = 0; slot < world_path_store::SLOT_COUNT; slot++) store.generations[slot] = 1;
    store.next_sequence = 1;
}

// SEARCH BUFFERS

// Open set ordering: lowest f = g + h, then lowest h, then oldest node. Total order, so the search is deterministic.
static bool path_node_less(const world_path_store& store, ui32 node_a, ui32 node_b)
{
    const path_search_node& a = store.nodes[node_a];
    const path_search_node& b = store.nodes[node_b];
    if (a.g + a.h != b.g + b.h) return a.g + a.h < b.g + b.h;
    if (a.h != b.h) return a.h < b.h;
    return node_a < node_b;
}

static void path_heap_set(world_path_store& store, ui32 pos, ui32 node)
{
    store.heap[pos] = node;
    store.nodes[node].heap_pos = pos;
}

static void path_heap_sift_up(world_path_store& store, ui32 pos)
{
    ui32 node = store.heap[pos];
    while (pos > 0 && path_node_less(store, node, store.heap[(pos - 1) / 2]))
    {
        path_heap_set(store, pos, store.heap[(pos - 1) / 2]);
        pos = (pos - 1) / 2;
    }
    path_heap_set(store, pos, node);
}

static void path_heap_sift_down(world_path_store& store, ui32 pos)
{
    ui32 node = store.heap[pos];
    for (;;)
    {
        ui32 child = pos * 2 + 1;
        if (child >= store.heap_count) break;
        if (child + 1 < store.heap_count && path_node_less(store, store.heap[child + 1], store.heap[child])) child++;
        if (!path_node_less(store, store.heap[child], node)) break;
        path_heap_set(store, pos, store.heap[child]);
        pos = child;
    }
    path_heap_set(store, pos, node);
}

// Inserts the node in the open heap, or moves it up if already in it (its cost must only have decreased).
static void path_heap_push(world_path_store& store, ui32 node)
{
    if (store.nodes[node].heap_pos == PATH_NONE)
    {
        store.nodes[node].heap_pos = store.heap_count;
        store.heap[store.heap_count++] = node;
    }
    path_heap_sift_up(store, store.nodes[node].heap_pos);
}

static ui32 path_heap_pop(world_path_store& store)
{
    ui32 node = store.heap[0];
    store.nodes[node].heap_pos = PATH_NONE;
    store.heap_count--;
    if (store.heap_count > 0)
    {
        path_heap_set(store, 0, store.heap[store.heap_count]);
        path_heap_sift_down(store, 0);
    }
    return node;
}

// Finds the node of a tile. Returns PATH_NONE if the tile has none yet, in which case out_hash_pos is where to register it.
static ui32 path_node_find(const world_path_store& store, world_location location, ui32& out_hash_pos)
{
    ui32 pos = ((ui32)location.x * 73856093u ^ (ui32)location.y * 19349663u) & (world_path_store::SEARCH_HASH_SIZE - 1);
    while (store.hash[pos] != 0)
    {
        if (store.nodes[store.hash[pos] - 1].location == location)
        {
            out_hash_pos = pos;
            return store.hash[pos] - 1;
        }
        pos = (pos + 1) & (world_path_store::SEARCH_HASH_SIZE - 1);
    }
    out_hash_pos = pos;
    return PATH_NONE;
}

// RULES

static bool path_tile_is_water(const world_terrain& terrain, ui16 region_index, ui16 tile_index)
{
    return terrain.regions[region_index].terrain_types[tile_index] >= TERRAIN_TILE_TYPE::WATER_COAST;
}

// Whether the tile can be walked on under the params. If so, out_extra_cost is the extra cost to cross it on top of its travel cost.
static bool path_tile_walkable(const match_world_state& world, const path_params& params, ui16 region_index, ui16 tile_index, ui32& out_extra_cost)
{
    const world_terrain& terrain = world.terrain;
    out_extra_cost = 0;
    if (path_tile_is_water(terrain, region_index, tile_index) != params.maritime) return false;

    ui16 owner = terrain.regions[region_index].owners[tile_index];
    if (owner == world_terrain::UNOWNED_TILE)
    {
        out_extra_cost = params.wilderness_extra_cost;
        return params.allow_wilderness;
    }
    if (params.only_settlement.is_valid() && owner != params.only_settlement.get_index()) return false;
    if (params.only_player != INVALID_MATCH_PLAYER_ID && world.entities.settlements.owners[owner] != params.only_player) return false;
    return true;
}

// Chamfer distance times the lowest possible tile cost, so it never overestimates the real cost.
static ui32 path_heuristic(world_location from, world_location to)
{
    ui32 dx = from.x > to.x ? from.x - to.x : to.x - from.x;
    ui32 dy = from.y > to.y ? from.y - to.y : to.y - from.y;
    ui32 major = dx > dy ? dx : dy;
    ui32 minor = dx > dy ? dy : dx;
    return (major * PROXIMITY_WEIGHT_ORTHOGONAL + minor * (PROXIMITY_WEIGHT_DIAGONAL - PROXIMITY_WEIGHT_ORTHOGONAL)) * TILE_ROAD_TRAVEL_COST;
}

// STORED PATHS

static void path_slot_release(world_path_store& store, ui32 slot)
{
    store.sequences[slot] = 0;
    store.generations[slot]++;
    if (store.generations[slot] == 0) store.generations[slot] = 1;
}

// Returns a free slot, evicting the oldest path if there is none.
static ui32 path_slot_acquire(world_path_store& store)
{
    ui32 slot = 0;
    for (ui32 i = 0; i < world_path_store::SLOT_COUNT; i++)
    {
        if (store.sequences[i] == 0) { slot = i; break; }
        if (store.sequences[i] < store.sequences[slot]) slot = i;
    }
    if (store.sequences[slot] != 0) path_slot_release(store, slot);
    store.sequences[slot] = store.next_sequence++;
    return slot;
}

bool path_get(const match_world_state& world, path_handle handle, path_view& out_view)
{
    const world_path_store& store = *world.paths;
    if (handle.slot >= world_path_store::SLOT_COUNT) return false;
    if (store.sequences[handle.slot] == 0 || store.generations[handle.slot] != handle.generation) return false;

    out_view.status = store.statuses[handle.slot];
    out_view.cost = store.costs[handle.slot];
    out_view.tile_count = store.tile_counts[handle.slot];
    out_view.tiles = &store.tiles[handle.slot * world_path_store::SLOT_TILE_CAPACITY];
    return true;
}

void path_free(match_world_state& world, path_handle handle)
{
    path_view view;
    if (path_get(world, handle, view)) path_slot_release(*world.paths, handle.slot);
}

static bool path_params_equal(const path_params& a, const path_params& b)
{
    return a.maritime == b.maritime && a.allow_wilderness == b.allow_wilderness && a.wilderness_extra_cost == b.wilderness_extra_cost
        && a.only_settlement == b.only_settlement && a.only_player == b.only_player && a.max_nodes == b.max_nodes;
}

// Finds a path stored in the current epoch for exactly this request. Returns false if there is none.
static bool path_find_stored(const world_path_store& store, const world_location* starts, ui32 start_count, world_location target,
        const path_params& params, path_handle& out_handle)
{
    if (start_count > world_path_store::SLOT_TILE_CAPACITY) return false;

    for (ui32 slot = 0; slot < world_path_store::SLOT_COUNT; slot++)
    {
        if (store.sequences[slot] == 0 || store.epochs[slot] != store.epoch) continue;
        if (store.request_start_counts[slot] != start_count || !(store.request_targets[slot] == target)) continue;
        if (!path_params_equal(store.requests[slot], params)) continue;

        const world_location* slotStarts = &store.request_starts[slot * world_path_store::SLOT_TILE_CAPACITY];
        bool sameStarts = true;
        for (ui32 i = 0; i < start_count && sameStarts; i++) sameStarts = slotStarts[i] == starts[i];
        if (!sameStarts) continue;

        out_handle = { (ui16)slot, store.generations[slot] };
        return true;
    }
    return false;
}

// A*

path_handle path_find(const match_world_state& world, const world_location* starts, ui32 start_count, world_location target, const path_params& params)
{
    const path_handle NO_PATH = { 0, 0 };
    world_path_store& store = *world.paths;
    const world_terrain& terrain = world.terrain;

    path_handle storedHandle;
    if (path_find_stored(store, starts, start_count, target, params, storedHandle)) return storedHandle;

    if (target.x >= terrain.size_tiles.x || target.y >= terrain.size_tiles.y) return NO_PATH;
    if (params.only_settlement.is_valid() && !world_entity_is_valid(world, params.only_settlement)) return NO_PATH;

    ui32 nodeLimit = params.max_nodes < world_path_store::SEARCH_NODE_CAPACITY ? params.max_nodes : world_path_store::SEARCH_NODE_CAPACITY;
    ia_memzero(store.hash, sizeof(store.hash));
    store.node_count = 0;
    store.heap_count = 0;

    // Start tiles, all at cost 0. Also tracks the explored node closest to the target, which is the path's end if the target is not reached.
    ui32 bestNode = PATH_NONE;
    for (ui32 i = 0; i < start_count && store.node_count < nodeLimit; i++)
    {
        if (starts[i].x >= terrain.size_tiles.x || starts[i].y >= terrain.size_tiles.y) continue;

        ui16 regionIndex, tileIndex;
        world_get_tile(terrain, starts[i], regionIndex, tileIndex);
        ui32 extraCost;
        if (!path_tile_walkable(world, params, regionIndex, tileIndex, extraCost)) continue;

        ui32 hashPos;
        if (path_node_find(store, starts[i], hashPos) != PATH_NONE) continue; // Duplicate start.

        ui32 node = store.node_count++;
        store.nodes[node] = { starts[i], 0, path_heuristic(starts[i], target), PATH_NONE, PATH_NONE };
        store.hash[hashPos] = node + 1;
        path_heap_push(store, node);
        if (bestNode == PATH_NONE || store.nodes[node].h < store.nodes[bestNode].h) bestNode = node;
    }
    if (bestNode == PATH_NONE) return NO_PATH;

    bool targetReached = false;
    ui32 endNode = bestNode;
    while (store.heap_count > 0)
    {
        ui32 node = path_heap_pop(store);
        if (store.nodes[node].location == target)
        {
            targetReached = true;
            endNode = node;
            break;
        }

        const world_location nodeLocation = store.nodes[node].location;
        const ui32 nodeCost = store.nodes[node].g;
        bool outOfNodes = false;
        for (ui8 offsetIndex = 0; offsetIndex < 8; offsetIndex++)
        {
            const vec2<i8>& offset = NEIGHBOUR_OFFSETS_ALL[offsetIndex];
            ui16 regionIndex, tileIndex;
            if (!world_get_neighbour_tile(terrain, nodeLocation, offset, regionIndex, tileIndex)) continue;

            ui32 extraCost;
            if (!path_tile_walkable(world, params, regionIndex, tileIndex, extraCost)) continue;

            world_location neighbour = { (ui16)(nodeLocation.x + offset.x), (ui16)(nodeLocation.y + offset.y) };
            bool isDiagonal = offset.x != 0 && offset.y != 0;
            ui32 stepCost = (world_get_tile_travel_cost(terrain, regionIndex, tileIndex) + extraCost)
                * (isDiagonal ? PROXIMITY_WEIGHT_DIAGONAL : PROXIMITY_WEIGHT_ORTHOGONAL);
            ui32 newCost = nodeCost + stepCost;

            ui32 hashPos;
            ui32 neighbourNode = path_node_find(store, neighbour, hashPos);
            if (neighbourNode == PATH_NONE)
            {
                if (store.node_count >= nodeLimit)
                {
                    outOfNodes = true;
                    break;
                }
                neighbourNode = store.node_count++;
                store.nodes[neighbourNode] = { neighbour, newCost, path_heuristic(neighbour, target), node, PATH_NONE };
                store.hash[hashPos] = neighbourNode + 1;
                if (store.nodes[neighbourNode].h < store.nodes[bestNode].h) bestNode = neighbourNode;
            }
            else
            {
                if (newCost >= store.nodes[neighbourNode].g) continue; // Strictly better only, so the first found path wins ties.
                store.nodes[neighbourNode].g = newCost;
                store.nodes[neighbourNode].parent = node;
            }
            path_heap_push(store, neighbourNode);
        }
        if (outOfNodes) break;
    }
    if (!targetReached) endNode = bestNode;

    // Store the path in a slot. Paths longer than a slot are cut after SLOT_TILE_CAPACITY tiles from the start, and are then partial too.
    ui32 length = 1;
    for (ui32 node = endNode; store.nodes[node].parent != PATH_NONE; node = store.nodes[node].parent) length++;
    ui32 storedCount = length < world_path_store::SLOT_TILE_CAPACITY ? length : world_path_store::SLOT_TILE_CAPACITY;

    ui32 slot = path_slot_acquire(store);
    world_location* slotTiles = &store.tiles[slot * world_path_store::SLOT_TILE_CAPACITY];
    ui32 index = length;
    for (ui32 node = endNode;; node = store.nodes[node].parent)
    {
        index--;
        if (index < storedCount)
        {
            slotTiles[index] = store.nodes[node].location;
            if (index == storedCount - 1) store.costs[slot] = store.nodes[node].g;
        }
        if (store.nodes[node].parent == PATH_NONE) break;
    }
    store.epochs[slot] = store.epoch;
    store.requests[slot] = params;
    store.request_targets[slot] = target;
    if (start_count <= world_path_store::SLOT_TILE_CAPACITY)
    {
        store.request_start_counts[slot] = start_count;
        ia_memcpy(&store.request_starts[slot * world_path_store::SLOT_TILE_CAPACITY], starts, start_count * sizeof(world_location));
    }
    else
    {
        store.request_start_counts[slot] = ~0u;
    }
    store.tile_counts[slot] = storedCount;
    store.statuses[slot] = (targetReached && storedCount == length) ? PATH_COMPLETE : PATH_PARTIAL;
    return { (ui16)slot, store.generations[slot] };
}
