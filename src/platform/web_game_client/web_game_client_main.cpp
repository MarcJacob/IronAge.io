// Entry point for the Web WASM client compilation and execution.
// Owns everything WASM-specific: the WASM_EXPORT macro, the exported wrapper functions that marshal calls into
// the client backend, and the static memory block the backend lives in. The backend itself (src/client_web/)
// knows nothing about WASM.

#include "core.h"

#include "core/assert.h"
#include "game_client/game_client_backend.h"
#include "game_common/match/world.h"
#include "web_game_client.h"

// Unity-compile the client backend.
#include "../../game_client/game_client_main.cpp"

// Unity-compile components of the WASM platform code.
#include "web_game_client_input.cpp"
#include "web_game_client_query.cpp"

web_client_state WEB_CLIENT;
_ASSERTION_HANDLER* _ASSERTION_HANDLER_PTR;

static constexpr ui64 CLIENT_MEMORY_SIZE = MiB(128);
ui8 CLIENT_MEMORY_BLOCK[CLIENT_MEMORY_SIZE];


// Platform implementation of the core assertion functions: no way to report a message here, so just trap.
void ASSERT_EXIT_FUNC() { __builtin_trap(); }
void ASSERT_MSG_FUNC(const char* assertMsg, const char* filename, ui32 line, ...) { __builtin_trap(); }
WASM_EXPORT bool web_client_start()
{
	WEB_CLIENT = {};

    // Define assertion handler.
    WEB_CLIENT.assertion_handler.ASSERT_EXIT_FUNC = ASSERT_EXIT_FUNC;
    WEB_CLIENT.assertion_handler.ASSERT_MSG_FUNC = ASSERT_MSG_FUNC;
    _ASSERTION_HANDLER_PTR = &WEB_CLIENT.assertion_handler;

	// Initialize backend.
	WEB_CLIENT.backend_memory = mem_arena_create(CLIENT_MEMORY_BLOCK, CLIENT_MEMORY_SIZE);
	WEB_CLIENT.backend = game_client_init(WEB_CLIENT.backend_memory);
	
	return WEB_CLIENT.backend != nullptr;
}

WASM_EXPORT void web_client_tick(float delta_time)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	game_client_tick(*WEB_CLIENT.backend, delta_time);
}

WASM_EXPORT world_dimensions* web_client_get_world_size()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return &game_client_get_render_state(*WEB_CLIENT.backend)->world_size;
}

WASM_EXPORT match_player_id web_client_get_controlled_player_index()
{
    ASSERT(WEB_CLIENT.backend != nullptr);
    return WEB_CLIENT.backend->controlled_player_id;
}

// Render state reading

WASM_EXPORT client_render_state::viewport_state* client_render_state_get_viewport()
{
    ASSERT(WEB_CLIENT.backend != nullptr);
    return &game_client_get_render_state(*WEB_CLIENT.backend)->viewport;   
}

WASM_EXPORT render_entity* client_render_state_get_entities()
{
    ASSERT(WEB_CLIENT.backend != nullptr);
    return game_client_get_render_state(*WEB_CLIENT.backend)->frame.entity_states;
}

WASM_EXPORT ui32 client_render_state_get_entity_count()
{
    ASSERT(WEB_CLIENT.backend != nullptr);
    return game_client_get_render_state(*WEB_CLIENT.backend)->frame.entity_count;
}

WASM_EXPORT ui8* client_render_state_get_terrain_tiles()
{
    ASSERT(WEB_CLIENT.backend != nullptr);
    return (ui8*)game_client_get_render_state(*WEB_CLIENT.backend)->tiles.terrain_tiles;
}

WASM_EXPORT ui8* client_render_state_get_influence_tiles()
{
    ASSERT(WEB_CLIENT.backend != nullptr);
    return game_client_get_render_state(*WEB_CLIENT.backend)->tiles.influence_tiles_bitmap;
}

// Net message handling. JS writes a received websocket message's bytes into the buffer, then calls client_process_net_message.

WASM_EXPORT ui8* client_get_net_message_buffer_offset()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_get_net_msg_buffer(*WEB_CLIENT.backend);
}

WASM_EXPORT ui32 client_get_net_message_buffer_size()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_get_net_msg_buffer_size(*WEB_CLIENT.backend);
}

// Parses and applies the message currently in the net message buffer. Returns the handled message type,
// or GAME_MESSAGE_TYPE::TYPE_COUNT / INVALID if the message could not be processed.
// May build a CLIENT_TICK message from pending input in response (see client_get_net_output_message_size).
WASM_EXPORT GAME_MESSAGE_TYPE client_process_net_message(ui32 message_size)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_process_game_message(*WEB_CLIENT.backend, message_size);
}

// Outgoing message built by the last client_process_net_message call, if any. JS should check the size and,
// if non-zero, send that many bytes from the buffer over the websocket right after every processed message.
WASM_EXPORT ui8* client_get_net_output_message_buffer_offset()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_get_net_msg_output(*WEB_CLIENT.backend);
}

WASM_EXPORT ui32 client_get_net_output_message_size()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_get_net_msg_output_size(*WEB_CLIENT.backend);
}
