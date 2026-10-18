// Implementation for the "input bridging code" between JS frontend and Client backend.
// General Input Events made of a code and a fixed-size payload are sent from the JS at anytime. JS writes the
// payload bytes into INPUT_EVENT_BUFFER (below, sized to the backend's INPUT_EVENT_MAX_PAYLOAD_SIZE) then calls
// client_send_input_event(code) - the buffer's ownership/location is a platform concern, the backend just gets
// handed a pointer.

#include "game_client/game_client_backend.h"
#include "web_game_client.h"

static ui8 INPUT_EVENT_BUFFER[INPUT_EVENT_MAX_PAYLOAD_SIZE];

WASM_EXPORT ui8* client_get_input_event_buffer_offset()
{
	return INPUT_EVENT_BUFFER;
}

WASM_EXPORT ui32 client_get_input_event_buffer_size()
{
	return INPUT_EVENT_MAX_PAYLOAD_SIZE;
}

WASM_EXPORT bool client_send_input_event(INPUT_EVENT_TYPE code)
{
	return game_client_process_input_event(*WEB_CLIENT.backend, code, INPUT_EVENT_BUFFER);
}

// Queues the match command (MATCH_COMMAND_TYPE value) whose payload JS wrote into the input event buffer.
// The buffer is read as the command's payload layout (see commands.h), not as an input event. Same layout as the validity query below.
WASM_EXPORT bool client_send_match_command(ui32 match_command_type)
{
	return game_client_input_push_match_command(*WEB_CLIENT.backend, (ui8)match_command_type, INPUT_EVENT_BUFFER);
}

// Asks whether the command (MATCH_COMMAND_TYPE value) whose payload JS wrote into the input event buffer would be valid right now.
// The buffer is read as the command's payload layout (see commands.h), not as an input event.
WASM_EXPORT bool client_query_command_valid(ui32 command_type)
{
	return game_client_query_command_valid(*WEB_CLIENT.backend, (ui8)command_type, INPUT_EVENT_BUFFER);
}

