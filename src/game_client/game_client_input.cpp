// Implementation for client backend input state handling.

#include "core.h"

#include "game_client.h"

static constexpr float VIEWPORT_PAN_SPEED_FRACTION = 0.5f; // Fraction of current viewport size crossed per second, at full pan input.
static constexpr float VIEWPORT_ZOOM_EASE_RATE = 3.0f; // How fast zoom_level eases toward its target, per second.

void game_client_input_set_target_loc(game_client& backend, entity_guid entity, int viewport_x, int viewport_y)
{
	if (backend.local_match == nullptr) return;

	// Viewport space -> world location, rounded to a tile and clamped inside the world.
	const client_viewport_state& viewport = backend.player_viewport;
	float worldX = ia_max(0.0f, ia_min(viewport.view_rect_min.x + (float)viewport_x, (float)viewport.world_size.x - 1.0f));
	float worldY = ia_max(0.0f, ia_min(viewport.view_rect_min.y + (float)viewport_y, (float)viewport.world_size.y - 1.0f));

	auto* payload = backend.input.command_queue_builder.push_command<command_data_set_entity_move_target>(MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET);
	if (payload == nullptr) return; // Queue full - drop the command.

	payload->entity = entity;
	payload->move_target = { (ui16)(worldX + 0.5f), (ui16)(worldY + 0.5f) };
}

void game_client_input_found_settlement(game_client& backend, entity_guid army)
{
	if (backend.local_match == nullptr) return;

	auto* payload = backend.input.command_queue_builder.push_command<command_data_found_settlement>(MATCH_COMMAND_TYPE::FOUND_SETTLEMENT);
	if (payload == nullptr) return; // Queue full - drop the command.

	payload->army = army;
}

void game_client_input_spawn_army(game_client& backend, entity_guid settlement)
{
	if (backend.local_match == nullptr) return;

	auto* payload = backend.input.command_queue_builder.push_command<command_data_spawn_army>(MATCH_COMMAND_TYPE::SPAWN_ARMY);
	if (payload == nullptr) return; // Queue full - drop the command.

	payload->settlement = settlement;
}

void game_client_input_spawn_caravan(game_client& backend, entity_guid settlement)
{
	if (backend.local_match == nullptr) return;

	auto* payload = backend.input.command_queue_builder.push_command<command_data_spawn_caravan>(MATCH_COMMAND_TYPE::SPAWN_CARAVAN);
	if (payload == nullptr) return; // Queue full - drop the command.

	payload->settlement = settlement;
}

void game_client_input_attack_target(game_client& backend, entity_guid attacker_entity, entity_guid target_entity)
{
	if (backend.local_match == nullptr) return;

	auto* payload = backend.input.command_queue_builder.push_command<command_data_set_entity_attack_target>(MATCH_COMMAND_TYPE::SET_ENTITY_ATTACK_TARGET);
	if (payload == nullptr) return; // Queue full - drop the command.

	payload->attacker_entity = attacker_entity;
	payload->target_entity = target_entity;
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
	case INPUT_EVENT_TYPE::FOUND_SETTLEMENT:
	{
		auto& payload = *(input_event_payload_found_settlement*)payload_bytes;
		game_client_input_found_settlement(backend, payload.army);
		return true;
	}
	case INPUT_EVENT_TYPE::SPAWN_ARMY:
	{
		auto& payload = *(input_event_payload_spawn_army*)payload_bytes;
		game_client_input_spawn_army(backend, payload.settlement);
		return true;
	}
	case INPUT_EVENT_TYPE::ATTACK_TARGET:
	{
		auto& payload = *(input_event_payload_attack_target*)payload_bytes;
		game_client_input_attack_target(backend, payload.attacker_entity, payload.target_entity);
		return true;
	}
	case INPUT_EVENT_TYPE::SPAWN_CARAVAN:
	{
		auto& payload = *(input_event_payload_spawn_caravan*)payload_bytes;
		game_client_input_spawn_caravan(backend, payload.settlement);
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
