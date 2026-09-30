// Main implementation file for the web client backend. Pure C++ logic - no WASM-specific concerns; see
// src/platform/wasm_client_main.cpp for the WASM export wrappers that drive this from JS.

#include "core.h"
#include "game_client/game_client_backend.h"

#include "game_client.h"

// Unity-compile the Game Common code into the web client.
#include "../game_common/game_common_main.cpp"

// Unity-compile the rest of the backend.
#include "game_client_net.cpp"
#include "game_client_render.cpp"
#include "game_client_input.cpp"

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
	backend->render_memory = mem_arena_create_sub(*backend->memory, RENDER_MEM_SIZE);

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
		game_client_rebuild_render_state(backend.render_state, backend.render_memory, *backend.local_match,
			backend.player_viewport, backend.controlled_player_id);
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
	backend.player_viewport.world_size = params.world_dimensions;
	return true;
}
