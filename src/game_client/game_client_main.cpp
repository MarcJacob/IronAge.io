// Main implementation file for the web client backend. Pure C++ logic - no WASM-specific concerns; see
// src/platform/wasm_client_main.cpp for the WASM export wrappers that drive this from JS.

#include "core.h"
#include "core/memory.h"
#include "game_client/game_client_backend.h"

#include "game_client.h"

// Unity-compile the Game Common code into the web client.
#include "../game_common/game_common_main.cpp"

// Unity-compile the rest of the backend.
#include "game_client_net.cpp"
#include "game_client_render.cpp"
#include "game_client_input.cpp"
#include "game_client_query.cpp"

game_client* game_client_init(mem_arena& backend_memory)
{
	ui64 memoryAvailable = backend_memory.mem_size - backend_memory.allocated_count;
	ASSERT_MSG(backend_memory.mem_start != nullptr && memoryAvailable > sizeof(game_client),
		"Client backend requires more than %llu bytes, got %llu.", sizeof(game_client), memoryAvailable);

	// Initialize backend, assign it its memory.
	game_client* backend = backend_memory.alloc<game_client>();
	*backend = {};

	backend->memory = &backend_memory;

	// Sub-allocate the match / render / input queue arenas once. Match (re)starts, render rebuilds, and input queue
	// drains all reuse this same backing memory via clear(), rather than sub-allocating again each time.
	static constexpr ui64 LOCAL_MATCH_MEM_SIZE = MiB(64);
	static constexpr ui64 RENDER_MEM_SIZE = KiB(64);
	static constexpr ui64 COMMAND_QUEUE_MEM_SIZE = KiB(1);

	backend->local_match_mem = mem_arena_create_sub(*backend->memory, LOCAL_MATCH_MEM_SIZE);

	backend->input.command_queue = mem_arena_create_sub(*backend->memory, COMMAND_QUEUE_MEM_SIZE);
	backend->input.command_queue_builder.target_mem = &backend->input.command_queue;
	ASSERT(backend->input.command_queue_builder.init());
	return backend;
}

void game_client_tick(game_client& backend, float delta_time)
{
	if (backend.local_match != nullptr)
	{
		game_client_apply_viewport_input(backend, delta_time);
		game_client_rebuild_render_state(backend.render_state, *backend.local_match);
	}
}

// Starts the local match with the given start parameters. params must already live in backend.local_match_mem
// (the caller copies it there before calling this) - this does NOT clear that arena or copy params again, since
// that would destroy params before match_start can read it.
bool game_client_begin_match_with_params(game_client& backend, game_match_start_params& params, ui32 params_size)
{
	game_match* localMatch = backend.local_match_mem.alloc<game_match>();
	if (!match_start(backend.local_match_mem, 0, params, *localMatch))
	{
		backend.local_match = nullptr;
		return false;
	}

	backend.local_match = localMatch;
	backend.render_state.world_size = params.world_size_regions * world_terrain::REGION_SIZE;

    ui64 tileCount = backend.render_state.world_size.x * backend.render_state.world_size.y;

    // Initialize render state memory blocks & bitmaps.
    backend.render_state.tiles.terrain_tiles = backend.memory->alloc<TERRAIN_TILE_TYPE>(tileCount);
    ASSERT(backend.render_state.tiles.terrain_tiles != nullptr);

    backend.render_state.tiles.influence_tiles_bitmap = backend.memory->alloc<ui8>(tileCount);
    ASSERT(backend.render_state.tiles.influence_tiles_bitmap != nullptr);

    // Copy match terrain (stored region by region) into the row-major terrain bitmap.
    {
        const world_terrain& terrain = localMatch->world->terrain;
        const ui16 width = backend.render_state.world_size.x;
        const ui16 height = backend.render_state.world_size.y;
        for (ui16 y = 0; y < height; y++)
        {
            for (ui16 x = 0; x < width; x++)
            {
                ui32 regionIndex = (y / world_terrain::REGION_SIZE) * terrain.size_regions.x + (x / world_terrain::REGION_SIZE);
                ui32 tileIndex = (y % world_terrain::REGION_SIZE) * world_terrain::REGION_SIZE + (x % world_terrain::REGION_SIZE);
                backend.render_state.tiles.terrain_tiles[(ui64)y * width + x] = terrain.regions[regionIndex].terrain_types[tileIndex];
            }
        }
    }
    ia_memset_32(backend.render_state.tiles.influence_tiles_bitmap, 0, tileCount);

    // Alloc per-frame memory.
    backend.render_state.frame.memory = mem_arena_create_sub(*backend.memory, MiB(16));

	return true;
}
