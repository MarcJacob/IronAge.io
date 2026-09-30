// Public interface for the web client's backend: match lifecycle, render state, net message handling.
// Pure C++ logic, no WASM-specific concerns - a host (currently the wasm platform, src/platform/wasm_client_main.cpp)
// drives it by calling these functions with a game_client the host owns the memory for.

#ifndef game_client_INCLUDED
#define game_client_INCLUDED

#include "core.h"
#include "game_common/game_match.h"
#include "game_common/game_messages.h"
#include "game_common/game_commands.h"

struct game_client;

// BEGIN CLIENT INPUT

// Sets current viewport control input state.
void game_client_set_viewport_input(game_client& backend, vec2<float> pan_vector, float zoom_delta, vec2f zoom_target);

// Queues a command to set the controlled player's entity target location.
void game_client_input_set_target_loc(game_client& backend, int x, int y);

// Applies current viewport input state onto the render viewport.
void game_client_apply_viewport_input(game_client& backend, float delta_time);

// Current state of input on the client. Contains both "latent state" properties like the viewport controls, and queued input events to be sent
// away to the server on the next opportunity.
struct game_client_input_state
{
	struct viewport
	{
		vec2f movement_vec;
		float zoom_level; // Current desired zoom level.
		vec2f zoom_target; // Normalized viewport-space coordinates for where to center (de)zooming.
	} viewport_control;

	// Queue of match commands built by input events (e.g. game_client_input_set_target_loc), drained into the next
	// CLIENT_TICK message by game_client_output_client_tick_message. command_queue_builder wraps command_queue and
	// always has a sequence ready to push commands into - reset (re-init'd) right after each drain.
	mem_arena command_queue;
	command_sequence_builder command_queue_builder;
};

// END CLIENT INPUT

// BEGIN CLIENT RENDER

// Contains information about what the client's viewport: where in the world it is located, its zoom level...
struct client_viewport_state
{
	vec2<ui16> world_size; // Total world size in tiles this viewport is viewing.

	vec2<float> bottom_left_corner; // World location of the viewport's bottom left corner. Float for smooth panning.
	float zoom_level; // Current desired normalized zoom level. The lower, the fewer world tiles are visible at once.

	vec2<ui16> viewport_size_min = {50, 50}; // Minimum dimensions of the viewport into the world at zoom_level = 0.
	vec2<ui16> viewport_size_max = {1000, 1000}; // Maximum dimensions of the viewport into the world at zoom_level = 1.
};

// Rebuilds Render State from the local match's current state and viewport. controlled_player_id and the
// match's world size are copied into render_state as-is (see client_render_state).
void game_client_rebuild_render_state(client_render_state& render_state, mem_arena& render_memory,
	const game_match& match, const client_viewport_state& viewport, match_player_id controlled_player_id);

// END CLIENT RENDER

// BEGIN LOCAL MATCH CONTROL FUNCTIONS

// Creates a local match state using the provided start parameters.
bool game_client_begin_match_with_params(game_client& backend, game_match_start_params& params, ui32 params_size);

// END LOCAL MATCH CONTROL FUNCTIONS

// BEGIN GAME MESSAGE FUNCTIONS

// Parses and applies the buffered game message bytes currently in backend. Returns the handled message type,
// or GAME_MESSAGE_TYPE::INVALID if the message could not be processed.
GAME_MESSAGE_TYPE game_client_process_game_message(game_client& backend, ui32 message_size);

// Fills in the client net output buffer with a message containing outstanding inputs to the sent to the server.
void game_client_output_client_tick_message(game_client& backend);

// END NET MESSAGE FUNCTIONS

// Main state structure of the game client backend.
// Implements the backend interface structure for use by the platform / frontend.
struct game_client
{
	mem_arena* memory; // All memory the client has to work with, provided by platform. 

	mem_arena local_match_mem; // Memory used for running the local match simulation.
	mem_arena render_memory; // Memory used to build & rebuild the render state continually.

	game_match* local_match; // Pointer to local match simulation if any.
	match_player_id controlled_player_id; // ID of controlled player in active match.

	game_client_input_state input; // State of input on the client, impacted by input calls from the frontend.
	client_viewport_state player_viewport; // State of the abstract viewport, used to determine what is in view and where when building the render state.

	client_render_state render_state; // Latest render state.

	static constexpr ui32 NET_MSG_BUFFER_SIZE = KiB(4);
	ui8 net_msg_buffer[NET_MSG_BUFFER_SIZE]; // Filled by the host with a received message's bytes before processing.

	static constexpr ui32 NET_OUTPUT_BUFFER_SIZE = KiB(1);
	ui8 net_output_buffer[NET_OUTPUT_BUFFER_SIZE]; // Holds at most one outgoing CLIENT_TICK message.
	ui32 net_output_msg_size; // 0 = nothing pending. Reset at the start of every game_client_process_game_message call.
};

#endif // game_client_INCLUDED
