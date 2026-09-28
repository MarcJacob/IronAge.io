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

// Render state exposed to the frontend.
struct web_client_render_state
{
	ui16 entity_count;
	match_world_state::entity* entity_states;
};

// Static / parameter data about the active match, exposed to the frontend.
struct web_client_match_info
{
	ui32 match_tick_rate;
	ui32 match_world_width, match_world_height;
};

#pragma pack(pop)

// Owns everything the client backend needs. A host places one of these at the start of a memory block it owns,
// via client_backend_init(), and passes it explicitly to every function below - no backend-owned statics.
struct client_backend
{
	mem_arena memory; // General arena; local_match_mem / render_memory sub-allocate from it, once, at init.
	mem_arena local_match_mem;
	mem_arena render_memory;

	game_match* local_match;
	match_player_id controlled_player_id;

	struct
	{
		i32 target_loc_x, target_loc_y;
		bool pending; // Set by client_backend_input_set_target_loc, cleared once built into an outgoing CLIENT_TICK message.
	} input;

	web_client_render_state render_state;
	web_client_match_info match_info;

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

// Starts a local match with built-in test parameters. For standalone / no-server testing only.
bool client_backend_begin_test_match(client_backend& backend);

void client_backend_input_set_target_loc(client_backend& backend, int x, int y);

// Ticks the local match with no commands. For standalone / no-server testing only.
void client_backend_tick_match(client_backend& backend);

// Parses and applies the message currently in backend.net_msg_buffer. Returns the handled message type,
// or GAME_MESSAGE_TYPE::TYPE_COUNT / INVALID if the message could not be processed. May build an outgoing
// CLIENT_TICK message into backend.net_output_buffer in response (see backend.net_output_msg_size).
GAME_MESSAGE_TYPE client_backend_process_net_message(client_backend& backend, ui32 message_size);

// Shared across the backend's own files (game_client_backend_*.cpp). Not part of the host-facing API.
bool client_backend_begin_match_with_params(client_backend& backend, game_match_start_params& params);
void client_backend_rebuild_render_state(client_backend& backend);
void client_backend_build_net_output_message(client_backend& backend);

#endif // GAME_CLIENT_BACKEND_INCLUDED
