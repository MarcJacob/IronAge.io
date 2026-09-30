// Simple implementation file for an AI player logic.
// AI players, like human players, observe the match state and create desired input commands in response to it and to their internal state,
// which doesn't have to follow the same rules of determinism as the match tick code since they live outside of it.

#include "game_common/match/AI_logic.h"

// Pushes a new command sequence for the AI's controlled player containing their desired input for the next match tick.
// Returns whether the AI could push all its commands successfully.
// NOTE: The commands are NOT checked for validity !
bool AI_output_commands(const game_match& match, AI_player_state& ai_state, match_tick_commands_builder& commands_builder)
{
	// Drive the controlled entity to a target location. Once it reaches that location, change it.

	// Inspect controlled entity state and determine whether we need to pick a new target location, then have the entity move there if not
	// doing so already.

	const match_world_state& world = match.world_state;
	const match_world_state::entity& controlledEntity = world.entity_states[ai_state.controlled_entity];

	float squaredDistToTarget = vec2_dist_squared(controlledEntity.location, controlledEntity.target_location);
	if (match.tick == 0 || squaredDistToTarget < 5.f)
	{
		// Pick new spot at random.
		world_location newTarget = ia_rand_vec(
			vec2f{ 0, 0 }, 
			vec2f {
				(float)match.start_params->world_dimensions.x,
				(float)match.start_params->world_dimensions.y
			});

		// Emit command.
		if (!commands_builder.push_new_sequence(ai_state.controlled_player)) return false;

		auto set_target_payload = commands_builder.push_command<command_data_set_entity_move_target>(MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET);
		if (set_target_payload == nullptr) return false;

		set_target_payload->new_target = newTarget;
		set_target_payload->target_entity = ai_state.controlled_entity;
	}

	return true;
}