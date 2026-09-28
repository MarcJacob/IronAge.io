// Main implementation file for the web client backend. Pure C++ logic - no WASM-specific concerns; see
// src/platform/wasm_client_main.cpp for the WASM export wrappers that drive this from JS.

#include "core.h"
#include "game_client_backend.h"

// Unity-compile the Game Common code into the web client.
#include "../game_common/game_common_main.cpp"

// Unity-compile the rest of the backend.
#include "game_client_backend_net.cpp"
#include "game_client_backend_render.cpp"

client_backend* client_backend_init(ui8* memory, ui64 memory_size)
{
	ASSERT_MSG(memory != nullptr && memory_size > sizeof(client_backend),
		"Client backend requires more than %llu bytes, got %llu.", sizeof(client_backend), memory_size);

	client_backend* backend = (client_backend*)memory;
	*backend = {};

	backend->memory = mem_arena_create(memory + sizeof(client_backend), memory_size - sizeof(client_backend));

	// Sub-allocate the match / render arenas once. Match (re)starts and render rebuilds reuse this same backing
	// memory via clear(), rather than sub-allocating again each time.
	static constexpr ui64 LOCAL_MATCH_MEM_SIZE = MiB(64);
	static constexpr ui64 RENDER_MEM_SIZE = KiB(64);

	backend->local_match_mem = mem_arena_create_sub(backend->memory, LOCAL_MATCH_MEM_SIZE);
	backend->render_memory = mem_arena_create_sub(backend->memory, RENDER_MEM_SIZE);

	return backend;
}

// Starts a local match in the backend's memory using the given (already arena-resident) params.
// Shared tail end of client_backend_begin_test_match() and the MATCH_JOINED handler.
bool client_backend_begin_match_with_params(client_backend& backend, game_match_start_params& params)
{
	game_match* localMatch = backend.local_match_mem.alloc<game_match>();
	if (!match_start(backend.local_match_mem, 0, params, *localMatch))
	{
		backend.local_match = nullptr;
		return false; // TODO(Marc): Error / logging system for web. I'm still not sure if WASM can call front-facing JS functions.
	}

	backend.local_match = localMatch;

	// (Re)Initialize render and local match info structures.
	backend.render_memory.clear();
	backend.render_state.entity_count = 0;

	backend.match_info.match_tick_rate = client_backend_get_local_match(backend).start_params->tick_rate;
	backend.match_info.match_world_width = client_backend_get_local_match(backend).start_params->world_dimensions.x;
	backend.match_info.match_world_height = client_backend_get_local_match(backend).start_params->world_dimensions.y;

	return true;
}

bool client_backend_begin_test_match(client_backend& backend)
{
	backend.local_match_mem.clear(); // Will override any previously-created match over the same memory, which is intended.
	game_match_start_params* matchParams = match_test_scenario_get_params(backend.local_match_mem);

	return client_backend_begin_match_with_params(backend, *matchParams);
}

void client_backend_input_set_target_loc(client_backend& backend, int x, int y)
{
	backend.input.target_loc_x = x;
	backend.input.target_loc_y = y;
	backend.input.pending = true;
}

void client_backend_tick_match(client_backend& backend)
{
	if (backend.local_match == nullptr) return;

	// TODO(Marc): external source (server messages or "local play" mode with direct output from client input to here).
	match_tick_commands tickCommands = {};
	match_tick(client_backend_get_local_match(backend), tickCommands);
	client_backend_rebuild_render_state(backend);
}
