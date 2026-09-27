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

static constexpr ui32 RENDER_MEMORY_SIZE = KiB(64);
static ui8 RENDER_MEMORY[RENDER_MEMORY_SIZE]; // Static memory pool backing CLIENT_BACKEND_STATE.render_memory.

// Static storage of client backend state.
struct
{
	mem_arena local_match_mem;
	mem_arena render_memory; // Cleared and rebuilt on every match tick to refresh render data for the browser.

	game_match* local_match;
	match_player_id controlled_player_id;

	struct
	{
		i32 target_loc_x, target_loc_y;
		bool pending; // Set on client_input_set_target_loc, cleared once built into an outgoing CLIENT_TICK message.
	} input;

} CLIENT_BACKEND_STATE;

// Static storage of what the frontend may be interested in for rendering the match state to the screen.
// Packed: JS reads this struct's fields directly at fixed byte offsets (see backend.js), which would otherwise
// shift depending on the compiler's padding around the pointer.
#pragma pack(push, 1)
struct web_client_render_state
{
	ui16 entity_count;
	match_world_state::entity* entity_states;

} CLIENT_RENDER_STATE;
#pragma pack(pop)

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

// Starts a local match in the client backend's memory using the given (already arena-resident) params.
// Shared tail end of client_begin_test_match() and the MATCH_JOINED handler.
static bool begin_match_with_params(game_match_start_params& params)
{
	// Alloc and place the match inside its own memory, as the first allocation.
	game_match* localMatch = CLIENT_BACKEND_STATE.local_match_mem.alloc<game_match>();
	if (!match_start(CLIENT_BACKEND_STATE.local_match_mem, 0, params, *localMatch))
	{
		CLIENT_BACKEND_STATE.local_match = nullptr;
		return false; // TODO(Marc): Error / logging system for web. I'm still not sure if WASM can call front-facing JS functions.
	}

	CLIENT_BACKEND_STATE.local_match = localMatch;

	// (Re)Initialize render and local match info structures.
	CLIENT_BACKEND_STATE.render_memory = mem_arena_create(RENDER_MEMORY, RENDER_MEMORY_SIZE);
	CLIENT_RENDER_STATE.entity_count = 0;

	LOCAL_MATCH_INFO.match_tick_rate = get_local_match().start_params->tick_rate;
	LOCAL_MATCH_INFO.match_world_width = get_local_match().start_params->world_dimensions.x;
	LOCAL_MATCH_INFO.match_world_height = get_local_match().start_params->world_dimensions.y;

	return true;
}

// Begins a local match in the client backend with built-in test parameters. For standalone / no-server testing only.
WASM_EXPORT bool client_begin_test_match()
{
	CLIENT_BACKEND_STATE.local_match_mem = mem_arena_create(CLIENT_MEMORY, CLIENT_MEMORY_SIZE); // Will override any previously-created arena over the same memory which is intended.
	game_match_start_params* matchParams = match_test_scenario_get_params(CLIENT_BACKEND_STATE.local_match_mem);

	return begin_match_with_params(*matchParams);
}

WASM_EXPORT void client_input_set_target_loc(int x, int y)
{
	CLIENT_BACKEND_STATE.input.target_loc_x = x;
	CLIENT_BACKEND_STATE.input.target_loc_y = y;
	CLIENT_BACKEND_STATE.input.pending = true;
}

// Rebuilds CLIENT_RENDER_STATE from the local match's current world state.
// TODO: Double buffering system ?
static void rebuild_render_state()
{
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

WASM_EXPORT void client_tick_match()
{
	if (CLIENT_BACKEND_STATE.local_match == nullptr) return;

	// TODO(Marc): external source (server messages or "local play" mode with direct output from client input to here).
	match_tick_commands tickCommands = {};
	match_tick(get_local_match(), tickCommands);
	rebuild_render_state();
}

WASM_EXPORT web_client_render_state* client_get_render_state()
{
	return &CLIENT_RENDER_STATE;
}

WASM_EXPORT web_client_match_info* client_get_local_match_info()
{
	return &LOCAL_MATCH_INFO;
}

WASM_EXPORT match_player_id client_get_controlled_player_id()
{
	return CLIENT_BACKEND_STATE.controlled_player_id;
}

// Net message handling.

// Reception buffer, copied into by the JS layer.
static constexpr ui32 NET_MSG_BUFFER_SIZE = KiB(4);
static ui8 NET_MSG_BUFFER[NET_MSG_BUFFER_SIZE];

WASM_EXPORT ui8* client_get_net_message_buffer()
{
	return NET_MSG_BUFFER;
}

WASM_EXPORT ui32 client_get_net_message_buffer_size()
{
	return NET_MSG_BUFFER_SIZE;
}

// Outgoing message buffer. Holds at most one CLIENT_TICK message, built in response to processing a SERVER_TICK.
// There is no separate send loop: JS is expected to check for a pending message right after every call to
// client_process_net_message and send it over the websocket if there is one.
static constexpr ui32 OUTGOING_MSG_BUFFER_SIZE = KiB(1);
static ui8 OUTGOING_MSG_BUFFER[OUTGOING_MSG_BUFFER_SIZE];
static ui32 OUTGOING_MSG_SIZE = 0; // 0 = nothing pending.

// Builds a CLIENT_TICK message out of the pending input, if any, into OUTGOING_MSG_BUFFER.
// Resets OUTGOING_MSG_SIZE to 0 first: a message is only ever offered once, right after it's built.
static void build_pending_output_message()
{
	OUTGOING_MSG_SIZE = 0;

	if (!CLIENT_BACKEND_STATE.input.pending) return;

	mem_arena outgoing_mem = mem_arena_create(OUTGOING_MSG_BUFFER, OUTGOING_MSG_BUFFER_SIZE);

	game_message_header* header = build_game_message(GAME_MESSAGE_TYPE::CLIENT_TICK, outgoing_mem,
		sizeof(match_command_header) + sizeof(command_payload_set_entity_move_target));
	ASSERT(header != nullptr);

	auto& payload = header->get_payload_ref<game_message_payload_client_tick>();
	payload.emit_tick = get_local_match().tick;
	payload.command_header_count = 1;

	match_command_header* commandHeader = (match_command_header*)payload._command_headers_buffer;
	*commandHeader = {};
	commandHeader->type = MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET;
#ifdef MATCH_COMMAND_DEBUG
	commandHeader->_data_size = sizeof(command_payload_set_entity_move_target);
#endif

	command_payload_set_entity_move_target& movePayload = commandHeader->get_command_payload<command_payload_set_entity_move_target>();
	movePayload.target_entity = CLIENT_BACKEND_STATE.controlled_player_id;
	movePayload.new_target = { (ui16)CLIENT_BACKEND_STATE.input.target_loc_x, (ui16)CLIENT_BACKEND_STATE.input.target_loc_y };

	OUTGOING_MSG_SIZE = sizeof(game_message_header) + header->payloadSize;
	CLIENT_BACKEND_STATE.input.pending = false;
}

WASM_EXPORT ui8* client_get_pending_output_message_buffer()
{
	return OUTGOING_MSG_BUFFER;
}

// Returns the size of the message built by the last client_process_net_message call, or 0 if there is none to send.
WASM_EXPORT ui32 client_get_pending_output_message_size()
{
	return OUTGOING_MSG_SIZE;
}

WASM_EXPORT GAME_MESSAGE_TYPE client_process_net_message(ui32 message_size)
{
	if (message_size < sizeof(game_message_header)) return GAME_MESSAGE_TYPE::TYPE_COUNT;

	game_message_header* header = (game_message_header*)NET_MSG_BUFFER;
	if (message_size < sizeof(game_message_header) + header->payloadSize) return GAME_MESSAGE_TYPE::TYPE_COUNT;

	switch (header->message_type)
	{
	case GAME_MESSAGE_TYPE::MATCH_JOINED:
	{
		auto& payload = header->get_payload_ref<game_message_payload_match_joined>();

		CLIENT_BACKEND_STATE.controlled_player_id = payload.controlled_player_id;

		// Copy the params (+ their trailing extra data) into the match's own memory: the net message buffer is reused for the next message.
		CLIENT_BACKEND_STATE.local_match_mem = mem_arena_create(CLIENT_MEMORY, CLIENT_MEMORY_SIZE);

		ui32 paramsBlockSize = sizeof(game_match_start_params) + payload.match_start_params.extra_data_size;
		ui8* paramsCopy = CLIENT_BACKEND_STATE.local_match_mem.alloc<ui8>(paramsBlockSize);
		ASSERT(paramsCopy != nullptr);
		ia_memcpy(paramsCopy, &payload.match_start_params, paramsBlockSize);

		if (!begin_match_with_params(*(game_match_start_params*)paramsCopy)) return GAME_MESSAGE_TYPE::TYPE_COUNT;

		break;
	}
	case GAME_MESSAGE_TYPE::SERVER_TICK:
	{
		if (CLIENT_BACKEND_STATE.local_match == nullptr) return GAME_MESSAGE_TYPE::TYPE_COUNT;

		auto& payload = header->get_payload_ref<game_message_payload_server_tick>();
		match_tick(get_local_match(), payload.commands);

		rebuild_render_state();
		build_pending_output_message();

		break;
	}
	default:
		return GAME_MESSAGE_TYPE::TYPE_COUNT;
	}

	return header->message_type;
}
