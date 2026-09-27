// Main implementation file for the web client backend.

#include "core.h"
#include "game_client_web.h"
#include "game_client_web_exports.h"

// Unity-compile the Game Common code into the web client.
#include "../game_common/game_common_main.cpp"

#define WEBCLIENT_INCLUDE_TEST_CODE 1
#if WEBCLIENT_INCLUDE_TEST_CODE

// Unity-compile test code.
#include "game_client_test_mode.cpp"

#endif

static constexpr ui32 CLIENT_MEMORY_SIZE = MiB(64);
static ui8 CLIENT_MEMORY[CLIENT_MEMORY_SIZE]; // Static memory pool for the client backend.

// Static storage of client backend state.
struct
{
	mem_arena local_match_mem;
	mem_arena render_memory; // Cleared and rebuilt on every match tick to refresh render data for the browser.

	game_match* local_match;

	struct
	{
		i32 target_loc_x, target_loc_y;
	} input;

} CLIENT_BACKEND_STATE;

// Static storage of what the frontend may be interested in for rendering the match state to the screen.
// TODO(Marc): This needs to turn into something dynamic that stores stuff in the pre-allocated client memory.
struct web_client_render_state
{
	ui16 entity_count;
	match_world_state::entity* entity_states;

} CLIENT_RENDER_STATE;

// Structure built from a live match, exposing relevant static / parameter data about the match in a way that is easy to read from the frontend.
struct web_client_match_info
{
	ui32 match_tick_rate;
	ui32 match_world_width, match_world_height;
} LOCAL_MATCH_INFO;

static inline game_match& get_local_match()
{
	ASSERT(CLIENT_BACKEND_STATE.local_match != nullptr);
	return *CLIENT_BACKEND_STATE.local_match;
}

// Begins a local match in the client backend with default parameters. Later this will need to read from parameters received from the server.
WASM_EXPORT bool client_begin_match()
{
	CLIENT_BACKEND_STATE.local_match_mem = mem_arena_create(CLIENT_MEMORY, CLIENT_MEMORY_SIZE); // Will override any previously-created arena over the same memory which is intended.
	game_match_start_params* matchParams = match_test_scenario_get_params(CLIENT_BACKEND_STATE.local_match_mem);

	// Alloc and place the match inside its own memory, as the first allocation.
	game_match* localMatch = CLIENT_BACKEND_STATE.local_match_mem.alloc<game_match>();
	if (!match_start(CLIENT_BACKEND_STATE.local_match_mem, 0, *matchParams, *localMatch))
	{
		CLIENT_BACKEND_STATE.local_match = nullptr;
		return false; // TODO(Marc): Error / logging system for web. I'm still not sure if WASM can call front-facing JS functions.
	}

	CLIENT_BACKEND_STATE.local_match = localMatch;

	// (Re)Initialize render and local match info structures.
	CLIENT_BACKEND_STATE.render_memory.clear();
	CLIENT_RENDER_STATE.entity_count = 0;

	LOCAL_MATCH_INFO.match_tick_rate = get_local_match().start_params->tick_rate;
	LOCAL_MATCH_INFO.match_world_width = get_local_match().start_params->world_dimensions.x;
	LOCAL_MATCH_INFO.match_world_height = get_local_match().start_params->world_dimensions.y;

	return true;
}

WASM_EXPORT void client_input_set_target_loc(int x, int y)
{
	CLIENT_BACKEND_STATE.input.target_loc_x = x;
	CLIENT_BACKEND_STATE.input.target_loc_y = y;
}

WASM_EXPORT void client_tick_match()
{
	if (CLIENT_BACKEND_STATE.local_match == nullptr) return;

	// TODO(Marc): external source (server messages or "local play" mode with direct output from client input to here).
	match_tick_commands tickCommands = {};
	match_tick(get_local_match(), tickCommands);

	// Rebuild render data.
	// TODO: Double buffering system ?

	mem_arena& renderMemory = CLIENT_BACKEND_STATE.render_memory;
	renderMemory.clear();

	match_world_state& world = get_local_match().world_state;

	CLIENT_RENDER_STATE.entity_count = world.entity_count;
	CLIENT_RENDER_STATE.entity_states = renderMemory.alloc<match_world_state::entity>(world.entity_count);

	for (entity_id entityID = 0; entityID < world.entity_count; entityID++)
	{
		// Straight copy should work so long as the entity structure remains a POD structure. Which it really should.
		match_world_state::entity& entity = CLIENT_RENDER_STATE.entity_states[entityID];
		entity = world.entity_states[entityID];
	}
}

WASM_EXPORT web_client_render_state* client_get_render_state()
{
	return &CLIENT_RENDER_STATE;
}

WASM_EXPORT web_client_match_info* client_get_local_match_info()
{
	return &LOCAL_MATCH_INFO;
}
