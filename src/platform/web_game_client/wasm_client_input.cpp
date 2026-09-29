// Implementation for the "input bridging code" between JS frontend and Client backend.
// General Input Events made of a code and a fixed-size payload are sent from the JS at anytime. The code here must read the code, the bytes,
// and output a proper input event structure to send to the backend so it may mutate input-related state.

#include "game_client/game_client_backend.h"
#include "wasm_client.h"

WASM_EXPORT void client_apply_viewport_input(float pan_x, float pan_y, float zoom_delta, float move_time,
	float cursor_viewport_frac_x, float cursor_viewport_frac_y)
{
	game_client_set_viewport_input(*WEB_CLIENT.backend, vec2f{ pan_x, pan_y }, zoom_delta, { cursor_viewport_frac_x, cursor_viewport_frac_y });
}

WASM_EXPORT void client_input_set_target_loc(int x, int y)
{
	game_client_input_set_target_loc(*WEB_CLIENT.backend, x, y);
}


