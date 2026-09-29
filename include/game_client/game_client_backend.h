// Main bridge between Game Client platform code and the platform-independent Game Client Backend.
// The backend is used as a service by the platform / frontend, by sending it input events, network messages to process...
// In return the backend can output abstract presentation data structures allowing the frontend to do its work.

#ifndef GAME_CLIENT_PLATFORM_INCLUDED
#define GAME_CLIENT_PLATFORM_INCLUDED

#include "core.h"

#include "game_common/game_match.h"
#include "game_common/game_messages.h"

// Structure definitions for resources sent from the client backend to frontend.

// Packed: made for external exposition, need to avoid padding.
#pragma pack(push, 1)

// Entity's location and target location in viewport space.
struct render_entity
{
	float viewport_x, viewport_y;
	float target_viewport_x, target_viewport_y;
};

// Render state exposed to the frontend.
struct client_render_state
{
	// Viewport space definition: bottom left corner location + width & height in world tiles.
	// Used to have an idea of the size of world elements compared to viewport.
	struct viewport_state
	{
		vec2<i32> viewport_bottom_left; // World-space coordinates of the viewport.
		ui16 viewport_width; // Width of the viewport in world tiles.
		ui16 viewport_height; // Height of the viewport in world tiles.
	} viewport;

	match_player_id controlled_player_id; // ID of the player this client is in control of.
	vec2<ui16> world_size; // Dimensions of the match world, in tiles. May move elsewhere once match info grows.

	ui16 entity_count; // Entities currently visible in the viewport only.
	render_entity* entity_states;
};

#pragma pack(pop)

// BACKEND INTERFACE DEFINITION.

struct game_client;
struct game_match_start_params;

// INPUT

// Temp. To be replaced by generalized input system.
void game_client_input_set_target_loc(game_client& backend, int x, int y);

// Updates the current viewport control input.
void game_client_set_viewport_input(game_client& backend, vec2<float> pan_vector, float zoom_delta, vec2f zoom_target);

// RENDERING 

// Gets current render state for presentation.
client_render_state* game_client_get_render_state(game_client& backend);

// MATCH

// If the backend has an ongoing local match, retrieves its start parameters.
game_match_start_params* game_client_get_local_match_params(game_client& backend);

// MESSAGING

// Processes the game message bytes present in the backend reception buffer and returns the processed message type (or INVALID if none).
GAME_MESSAGE_TYPE game_client_process_game_message(game_client& backend, ui32 message_size);

// Has the backend output its queued input actions in the form of a CLIENT_TICK game message in its messaging send buffer.
void game_client_output_client_tick_message(game_client& backend);

// Returns pointer to the backend game message reception buffer.
ui8* game_client_get_net_msg_buffer(game_client& backend);

// Returns size of the backend game message reception buffer.
ui32 game_client_get_net_msg_buffer_size(game_client& backend);

// Returns pointer to the backend game message output buffer.
ui8* game_client_get_net_msg_output(game_client& backend);

// Returns size of the backend game message output buffer.
ui32 game_client_get_net_msg_output_size(game_client& backend);

// MAIN LIFECYCLE FUNCTIONS.

// Initializes a client backend in the specified memory.
// From there the backend assumes it has full control of the passed memory arena.
// Returns a filled-in interface structure for use by the platform / front.
game_client* game_client_init(mem_arena& backend_memory);

// Ticks the backend logic by the provided real-time seconds.
void game_client_tick(game_client& backend, float delta_time);

#endif // GAME_CLIENT_PLATFORM_INCLUDED