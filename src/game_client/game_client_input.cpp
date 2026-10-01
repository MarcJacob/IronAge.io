// Implementation for client backend input state handling.

#include "core.h"

#include "game_client.h"

static constexpr float VIEWPORT_PAN_SPEED_FRACTION = 0.5f; // Fraction of current viewport size crossed per second, at full pan input.
static constexpr float VIEWPORT_ZOOM_EASE_RATE = 3.0f; // How fast zoom_level eases toward its target, per second.

void game_client_input_set_target_loc(game_client& backend, int x, int y)
{
	return;

	//auto* payload = backend.input.command_queue_builder.push_command<command_data_set_entity_move_target>(MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET);
	//if (payload == nullptr) return; // Queue full - drop the command.

	//payload->target_entity = backend.controlled_player_id;
	//payload->new_target = { (ui16)x, (ui16)y };
}

void game_client_set_viewport_input(game_client& backend, vec2f view_rect_min, vec2f view_rect_max)
{
	game_client_input_state::viewport& viewportInput = backend.input.viewport_control;

	viewportInput.view_rect_min = view_rect_min;
	viewportInput.view_rect_max = view_rect_max;
}

bool game_client_process_input_event(game_client& backend, INPUT_EVENT_TYPE code, ui8 payload_bytes[INPUT_EVENT_MAX_PAYLOAD_SIZE])
{
	switch (code)
	{
	case INPUT_EVENT_TYPE::VIEWPORT_CONTROL:
	{
		auto& payload = *(input_event_payload_viewport_control*)payload_bytes;
		game_client_set_viewport_input(backend,
			vec2f{ payload.view_rect_min_x, payload.view_rect_min_y },
			vec2f{ payload.view_rect_max_x, payload.view_rect_max_y });
		return true;
	}
	case INPUT_EVENT_TYPE::SET_TARGET_LOC:
	{
		auto& payload = *(input_event_payload_set_target_loc*)payload_bytes;
		game_client_input_set_target_loc(backend, payload.x, payload.y);
		return true;
	}
	default:
		return false;
	}
}

void game_client_apply_viewport_input(game_client& backend, float delta_time)
{
	client_viewport_state& viewport = backend.player_viewport;
	game_client_input_state::viewport& viewportInput = backend.input.viewport_control;

	viewport.view_rect_min = viewportInput.view_rect_min;
	viewport.view_rect_max = viewportInput.view_rect_max;
}
