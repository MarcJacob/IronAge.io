// Entry point for the Web WASM client compilation and execution.
// Owns everything WASM-specific: the WASM_EXPORT macro, the exported wrapper functions that marshal calls into
// the client backend, and the static memory block the backend lives in. The backend itself (src/client_web/)
// knows nothing about WASM.

// Only functions marked WASM_EXPORT are exported (build uses -fvisibility=hidden).
#define WASM_EXPORT extern "C" __attribute__((visibility("default")))

// Unity-compile the client backend.
#include "../../client_web/game_client_backend.h"
#include "../../client_web/game_client_backend_main.cpp"

#define WEBCLIENT_INCLUDE_TEST_CODE 1
#if WEBCLIENT_INCLUDE_TEST_CODE

// Unity-compile test code.
#include "wasm_client_test_mode.cpp"

#endif

// Static memory block the client backend lives in: the client_backend struct itself, followed by its arenas.
// Trivially zero-initialized (a plain static array, no constructor call) rather than initialized by calling
// client_backend_init() directly in a global initializer: this module is built with -Wl,--no-entry, so there's
// no guaranteed point at which C++ global constructors would run. get_backend() initializes lazily instead.
static constexpr ui64 CLIENT_ARENA_SIZE = MiB(64) + KiB(64); // Local match memory + render memory (see client_backend_init).
static ui8 CLIENT_MEMORY_BLOCK[sizeof(client_backend) + CLIENT_ARENA_SIZE];
static client_backend* BACKEND = nullptr;

static inline client_backend& get_backend()
{
	if (BACKEND == nullptr) BACKEND = client_backend_init(CLIENT_MEMORY_BLOCK, sizeof(CLIENT_MEMORY_BLOCK));
	return *BACKEND;
}

// Starts a local match with built-in test parameters. For standalone / no-server testing only.
WASM_EXPORT bool client_begin_test_match()
{
	return client_backend_begin_test_match(get_backend());
}

WASM_EXPORT void client_input_set_target_loc(int x, int y)
{
	client_backend_input_set_target_loc(get_backend(), x, y);
}

WASM_EXPORT void client_apply_viewport_input(float pan_x, float pan_y, float zoom_delta, float move_time,
	float cursor_viewport_frac_x, float cursor_viewport_frac_y)
{
	client_backend& backend = get_backend();
	vec2<ui16> worldSize = { (ui16)backend.match_info.match_world_width, (ui16)backend.match_info.match_world_height };
	client_backend_apply_viewport_input(backend.player_viewport, worldSize, { pan_x, pan_y }, zoom_delta, move_time,
		{ cursor_viewport_frac_x, cursor_viewport_frac_y });
}

// Ticks the local match with no commands. For standalone / no-server testing only.
WASM_EXPORT void client_tick_match()
{
	client_backend_tick_match(get_backend());
}

WASM_EXPORT void client_refresh_render_state()
{
	client_backend_refresh_render_state(get_backend());
}

WASM_EXPORT web_client_render_state* client_get_render_state()
{
	return &get_backend().render_state;
}

WASM_EXPORT web_client_match_info* client_get_local_match_info()
{
	return &get_backend().match_info;
}

WASM_EXPORT match_player_id client_get_controlled_player_id()
{
	return get_backend().controlled_player_id;
}

// Net message handling. JS writes a received websocket message's bytes into the buffer, then calls client_process_net_message.

WASM_EXPORT ui8* client_get_net_message_buffer_offset()
{
	return get_backend().net_msg_buffer;
}

WASM_EXPORT ui32 client_get_net_message_buffer_size()
{
	return client_backend::NET_MSG_BUFFER_SIZE;
}

// Parses and applies the message currently in the net message buffer. Returns the handled message type,
// or GAME_MESSAGE_TYPE::TYPE_COUNT / INVALID if the message could not be processed.
// May build a CLIENT_TICK message from pending input in response (see client_get_net_output_message_size).
WASM_EXPORT GAME_MESSAGE_TYPE client_process_net_message(ui32 message_size)
{
	return client_backend_process_net_message(get_backend(), message_size);
}

// Outgoing message built by the last client_process_net_message call, if any. JS should check the size and,
// if non-zero, send that many bytes from the buffer over the websocket right after every processed message.
WASM_EXPORT ui8* client_get_net_output_message_buffer_offset()
{
	return get_backend().net_output_buffer;
}

WASM_EXPORT ui32 client_get_net_output_message_size()
{
	return get_backend().net_output_msg_size;
}

// Platform implementation of the core assertion functions: no way to report a message here, so just trap.
void ASSERT_EXIT_FUNC() { __builtin_trap(); }
void ASSERT_MSG_FUNC(const char* assertMsg, const char* filename, ui32 line, ...) { __builtin_trap(); }
