// Pathfinding: SKELETON. The generalist pathfinder and the world's path store are stubs, to be written from scratch.

#include "game_common/match/world/world_state.h"
#include "game_common/match/world/world_terrain.h"
#include "world_movement.h"

void world_path_find(const world_terrain& terrain, const path_params& params, mem_arena& scratch, path& out_path)
{
    out_path.count = 0;
    out_path.partial = true;
    out_path.cost = 0;
}

world_path_store* world_path_store_init(mem_arena& memory)
{
    world_path_store* store = memory.alloc<world_path_store>();
    if (store == nullptr) return nullptr;

    ui8* scratchMemory = (ui8*)memory.alloc(WORLD_PATH_STORE_SCRATCH_SIZE);
    if (scratchMemory == nullptr) return nullptr;

    store->scratch = mem_arena_create(scratchMemory, WORLD_PATH_STORE_SCRATCH_SIZE);
    return store;
}

ui32 world_path_store_get_path(match_world_state& world, world_location start, world_location end, PATH_MODE mode)
{
    return INVALID_PATH_HANDLE;
}

bool world_path_store_move_entity(match_world_state& world, world_entity_movement& movement, world_location& location)
{
    return false;
}

world_location world_path_store_next_tile(match_world_state& world, ui32 path_handle, world_location from, world_location destination)
{
    return from;
}
