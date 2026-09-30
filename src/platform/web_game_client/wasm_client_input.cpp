// Implementation for the "input bridging code" between JS frontend and Client backend.
// General Input Events made of a code and a fixed-size payload are sent from the JS at anytime. JS writes the
// payload bytes into INPUT_EVENT_BUFFER (below, sized to the backend's INPUT_EVENT_MAX_PAYLOAD_SIZE) then calls
// client_send_input_event(code) - the buffer's ownership/location is a platform concern, the backend just gets
// handed a pointer.

#include "game_client/game_client_backend.h"
#include "wasm_client.h"

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

