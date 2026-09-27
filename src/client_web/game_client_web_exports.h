// Contains the declaration of all exported functions for the web frontend.

#include "game_client_web.h"
#include "game_common/game_messages.h"

struct web_client_render_state;
struct web_client_match_info;

// Starts a local match with built-in test parameters. For standalone / no-server testing only.
WASM_EXPORT bool client_begin_test_match();
WASM_EXPORT void client_input_set_target_loc(int x, int y);
WASM_EXPORT void client_tick_match();
WASM_EXPORT web_client_render_state* client_get_render_state();
WASM_EXPORT web_client_match_info* client_get_local_match_info();
WASM_EXPORT match_player_id client_get_controlled_player_id();

// Net message handling. JS writes a received websocket message's bytes into the buffer, then calls client_process_net_message.

WASM_EXPORT ui8* client_get_net_message_buffer();
WASM_EXPORT ui32 client_get_net_message_buffer_size();

// Parses and applies the message currently in the net message buffer. Returns the handled message type,
// or GAME_MESSAGE_TYPE::TYPE_COUNT if the message could not be processed.
// May build a CLIENT_TICK message from pending input in response (see client_get_pending_output_message_size).
WASM_EXPORT GAME_MESSAGE_TYPE client_process_net_message(ui32 message_size);

// Outgoing message built by the last client_process_net_message call, if any. JS should check the size and,
// if non-zero, send that many bytes from the buffer over the websocket right after every processed message.
WASM_EXPORT ui8* client_get_pending_output_message_buffer();
WASM_EXPORT ui32 client_get_pending_output_message_size();
