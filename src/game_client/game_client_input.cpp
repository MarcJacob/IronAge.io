// Implementation for client backend input state handling.

#include "core.h"

#include "game_client.h"
#include "game_client/game_client_backend.h"

static constexpr float VIEWPORT_PAN_SPEED_FRACTION = 0.5f; // Fraction of current viewport size crossed per second, at full pan input.
static constexpr float VIEWPORT_ZOOM_EASE_RATE = 3.0f; // How fast zoom_level eases toward its target, per second.

void game_client_input_set_target_loc(game_client& backend, entity_guid entity, int viewport_x, int viewport_y)
{
	if (backend.local_match == nullptr) return;

	// Viewport space -> world location, rounded to a tile and clamped inside the world.
	const client_render_state::viewport_state& viewport = backend.render_state.viewport;
	float worldX = ia_max(0.0f, ia_min(viewport.bottom_left.x + (float)viewport_x, (float)backend.render_state.world_size.x - 1.0f));
	float worldY = ia_max(0.0f, ia_min(viewport.bottom_left.y + (float)viewport_y, (float)backend.render_state.world_size.y - 1.0f));

	auto* payload = backend.input.command_queue_builder.push_command<command_data_set_entity_move_target>(MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET);
	if (payload == nullptr) return; // Queue full - drop the command.

	payload->entity = entity;
	payload->move_target = { (ui16)(worldX + 0.5f), (ui16)(worldY + 0.5f) };
}

bool game_client_input_push_match_command(game_client& backend, ui8 match_command_type, const ui8* payload_bytes)
{
	if (match_command_type >= (ui8)MATCH_COMMAND_TYPE::TYPE_COUNT) return false;

	MATCH_COMMAND_TYPE type = (MATCH_COMMAND_TYPE)match_command_type;
	ui8 payloadSize = get_command_data_size(type);
	if (payloadSize > INPUT_EVENT_MAX_PAYLOAD_SIZE) return false;

	if (backend.local_match == nullptr) return true;

	void* payload = backend.input.command_queue_builder.push_command(type);
	if (payload == nullptr) return true; // Queue full - drop the command.

	ia_memcpy(payload, payload_bytes, payloadSize);
	return true;
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
		game_client_input_set_target_loc(backend, payload.entity, payload.x, payload.y);
		return true;
	}
	default:
		return false;
	}
}

void game_client_apply_viewport_input(game_client& backend, float delta_time)
{
    client_render_state::viewport_state& viewport = backend.render_state.viewport;
	game_client_input_state::viewport& viewportInput = backend.input.viewport_control;

	viewport.bottom_left = viewportInput.view_rect_min;
	viewport.dimensions = viewportInput.view_rect_max - viewportInput.view_rect_min;
}
