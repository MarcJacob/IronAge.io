// Pathfinding: SKELETON. The generalist pathfinder and the world's path store are stubs, to be written from scratch.

#include "game_common/match/world/world_state.h"
#include "game_common/match/world/world_terrain.h"
#include "world_movement.h"

// General-purpose path finder. Must NEVER creates a new path in the store since it can be called from outside
// the world simulation.
void world_path_find(const match_world_state& world, const path_params& params, mem_arena& scratch, path& out_path)
{
    out_path.points = nullptr;
    out_path.point_count = 0;

    out_path.length = 0;
    out_path.cost = 0;
}

world_path_store* world_path_store_init(mem_arena& memory)
{
    world_path_store* store = memory.alloc<world_path_store>();
    if (store == nullptr) return nullptr;

    ui8* scratchMemory = (ui8*)memory.alloc(WORLD_PATH_STORE_SCRATCH_SIZE);
    if (scratchMemory == nullptr) return nullptr;

    store->search_scratch = mem_arena_create(scratchMemory, WORLD_PATH_STORE_SCRATCH_SIZE);
    return store;
}

world_path_store::path_handle world_path_store_get_path(match_world_state& world, const path_params& parameters)
{
    return INVALID_PATH_HANDLE;
}

bool world_path_store_move_entity(match_world_state& world, world_location& location, world_entity_movement& entity_movement)
{
    return false;
}

world_location world_path_store_next_tile(match_world_state& world, world_path_store::path_handle path_handle, world_location location, world_location destination)
{
    return location;
}
