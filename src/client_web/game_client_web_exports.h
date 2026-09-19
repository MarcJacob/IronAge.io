// Contains the declaration of all exported functions for the web frontend.

#include "game_client_web.h"

struct web_client_render_state;

WASM_EXPORT bool client_begin_match();
WASM_EXPORT void client_input_set_target_loc(int x, int y);
WASM_EXPORT void client_tick_match();
WASM_EXPORT web_client_render_state* client_get_render_state();
