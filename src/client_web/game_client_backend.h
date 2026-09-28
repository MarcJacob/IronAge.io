// Public interface for the web client's backend: match lifecycle, render state, net message handling.
// Pure C++ logic, no WASM-specific concerns - a host (currently the wasm platform, src/platform/wasm_client_main.cpp)
// drives it by calling these functions with a client_backend the host owns the memory for.

#ifndef GAME_CLIENT_BACKEND_INCLUDED
#define GAME_CLIENT_BACKEND_INCLUDED

#include "core.h"
#include "game_common/game_match.h"
#include "game_common/game_messages.h"

// Packed: read directly at fixed byte offsets by JS.
#pragma pack(push, 1)

// An entity's position relative to the viewport's bottom-left corner, in world tiles.
struct render_entity
{
	float viewport_x, viewport_y;
	float target_viewport_x, target_viewport_y;
};

// Render state exposed to the frontend.
// Exposed to JS: mirrored by hand in src/client_web/front/src/backend.ts's read_render_state(). Keep both in sync.
// NOTE(Marc): Need to look into automating the process of "mirroring" this structure to Typescript automatically somehow. Some sort of static code analysis could be helpful.
// In the meantime I can keep doing it by hand as long as it remains simple, then I can let the AI do it.
struct web_client_render_state
{
	// Viewport space definition: bottom left corner location + width & height in world tiles.
	struct
	{
		vec2<i32> viewport_bottom_left;
		ui16 viewport_width;
		ui16 viewport_height;
		float zoom_level;
	} viewport;

	ui16 entity_count; // Entities currently visible in the viewport only.
	render_entity* entity_states;
};

// Static / parameter data about the active match, exposed to the frontend.
// Exposed to JS: mirrored by hand in src/client_web/front/src/backend.ts's read_match_info(). Keep both in sync.
struct web_client_match_info
{
	ui32 match_tick_rate;
	ui32 match_world_width, match_world_height;
};

#pragma pack(pop)

// Contains information about what the client's viewport: where in the world it is located, its zoom level...
struct client_viewport_state
{
	vec2<float> bottom_left_corner; // World location of the viewport's bottom left corner. Float for smooth panning.
	float zoom_level; // Current desired normalized zoom level. The lower, the fewer world tiles are visible at once.

	vec2<ui16> viewport_size_min = {50, 50}; // Minimum dimensions of the viewport into the world at zoom_level = 0.
	vec2<ui16> viewport_size_max = {1000, 1000}; // Maximum dimensions of the viewport into the world at zoom_level = 1.
};

// Owns everything the client backend needs. A host places one of these at the start of a memory block it owns,
// via client_backend_init(), and passes it explicitly to every function below - no backend-owned statics.
struct client_backend
{
	mem_arena memory; // General arena; local_match_mem / render_memory sub-allocate from it, once, at init.
	mem_arena local_match_mem;
	mem_arena render_memory;

	game_match* local_match;

	match_player_id controlled_player_id; // ID of controlled player in active match.

	client_viewport_state player_viewport;

	struct
	{
		i32 target_loc_x, target_loc_y;
		bool pending;
	} input; // Written into by input system, cleared when sent away as CLIENT_TICK game message.

	web_client_render_state render_state; // Current render state presented to JS.
	web_client_match_info match_info; // Static match information based on the active match's parameters.

	static constexpr ui32 NET_MSG_BUFFER_SIZE = KiB(4);
	ui8 net_msg_buffer[NET_MSG_BUFFER_SIZE]; // Filled by the host with a received message's bytes before processing.

	static constexpr ui32 NET_OUTPUT_BUFFER_SIZE = KiB(1);
	ui8 net_output_buffer[NET_OUTPUT_BUFFER_SIZE]; // Holds at most one outgoing CLIENT_TICK message.
	ui32 net_output_msg_size; // 0 = nothing pending. Reset at the start of every client_backend_process_net_message call.
};

inline game_match& client_backend_get_local_match(client_backend& backend)
{
	ASSERT(backend.local_match != nullptr);
	return *backend.local_match;
}

// Places a client_backend at the start of the given memory and sets up its arenas over the rest of it.
client_backend* client_backend_init(ui8* memory, ui64 memory_size);

// BEGIN LOCAL MATCH CONTROL FUNCTIONS

// Starts a local match with built-in test parameters. For standalone / no-server testing only.
bool client_backend_begin_test_match(client_backend& backend);

// Creates a local match state using the provided start parameters.
bool client_backend_begin_match_with_params(client_backend& backend, game_match_start_params& params);

// Ticks the local match with no commands. For standalone / no-server testing only.
void client_backend_tick_match(client_backend& backend);

// END LOCAL MATCH CONTROL FUNCTIONS

// BEGIN RENDER FUNCTIONS

// Builds a fresh render state from current viewport and match state.
void client_backend_build_render_state(web_client_render_state& render_state, mem_arena& render_memory,
	const game_match& match, const client_viewport_state& viewport);

// Clears render_memory and calls client_backend_build_render_state from backend's own state. No-op if there's
// no active match. Meant to be called once per animation frame.
void client_backend_refresh_render_state(client_backend& backend);

// END RENDER FUNCTIONS

// BEGIN NET MESSAGE FUNCTIONS

// Parses and applies the message currently in backend.net_msg_buffer. Returns the handled message type,
// or GAME_MESSAGE_TYPE::TYPE_COUNT / INVALID if the message could not be processed. May build an outgoing
// CLIENT_TICK message into backend.net_output_buffer in response (see backend.net_output_msg_size).
GAME_MESSAGE_TYPE client_backend_process_net_message(client_backend& backend, ui32 message_size);
// Fills in the client net output buffer with a message containing outstanding inputs to the sent to the server.
void client_backend_build_net_output_message(client_backend& backend);

// END NET MESSAGE FUNCTIONS

// BEGIN INPUT FUNCTIONS

// Current viewport size in world tiles, interpolated between viewport_size_min/max by zoom_level.
vec2<float> client_backend_get_viewport_size(const client_viewport_state& viewport);

// Applies input to the viewport state. pan_vector is a normalized direction; zoom_delta is the new desired zoom
// level. Both need repeated calls (e.g. once per frame while held) to keep taking effect - pan speed is scaled
// by current viewport size, zoom eases toward its target, both scaled by move_time. world_size clamps the result.
void client_backend_apply_viewport_input(client_viewport_state& viewport, vec2<ui16> world_size,
	vec2<float> pan_vector, float zoom_delta, float move_time);

void client_backend_input_set_target_loc(client_backend& backend, int x, int y);

// END INPUT FUNCTIONS


#endif // GAME_CLIENT_BACKEND_INCLUDED
