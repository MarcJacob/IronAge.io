// Main implementation file for match input command functions.

#include "game_common/match/commands.h"
#include "game_common_internals.h"

// BEGIN COMMAND FUNCTION IMPLEMENTATIONS

// ENTITY SET MOVE TARGET

// Example of command sanity check function.
bool command_set_entity_move_target_validity_check(const game_match& match, match_player_id player, const command_data_set_entity_move_target& command)
{
	if (!match_entity_is_valid(match, command.entity)) return false;

	// Check that target coordinates are inside the world.
	if (command.move_target.x < 0 
		|| command.move_target.x > match.world->terrain.size_tiles.x
		|| command.move_target.y < 0 
		|| command.move_target.y > match.world->terrain.size_tiles.y)
	{
		return false;
	}

	ENTITY_TYPE entityType = command.entity.get_type();
	switch (entityType)
	{
		// With valid types, just need to check that player can control them.
	case ENTITY_TYPE::ARMY:
		return match.world->entities.armies.owners[command.entity.get_index()] == player;
	default:
		// Command invalid: entity can't move or can't be ordered to move somewhere.
		return false;
	}
}

// Example of a command apply function.
void command_set_entity_move_target_apply(game_match& match, match_player_id player, const command_data_set_entity_move_target& command)
{
	if (!match_entity_is_valid(match, command.entity)) return;

	ENTITY_TYPE entityType = command.entity.get_type();

	switch (entityType)
	{
		// With valid types, just need to check that player can control them.
	case ENTITY_TYPE::ARMY:
		match.world->entities.armies.movements[command.entity.get_index()].move_target = command.move_target;
		break;
	default:
		// Command invalid: entity can't move or can't be ordered to move somewhere.
		break;
	}

}

// FOUND SETTLEMENT

bool command_found_settlement_validity_check(const game_match& match, match_player_id player, const command_data_found_settlement& command)
{
	if (!match_entity_is_valid(match, command.army)) return false;
	if (command.army.get_type() != ENTITY_TYPE::ARMY) return false;
	if (match.world->entities.armies.owners[command.army.get_index()] != player) return false;

	// The army is kept if there is no room for the settlement.
	const world_entity_settlements& settlements = match.world->entities.settlements;
	return settlements.active_count < settlements.max_count;
}

// Spawns a settlement at the army's location with its owner and total manpower as population, then destroys the army.
void command_found_settlement_apply(game_match& match, match_player_id player, const command_data_found_settlement& command)
{
	// Checked again: earlier commands in the same tick may have destroyed this army or filled the last settlement slot.
	if (!match_entity_is_valid(match, command.army)) return;
	if (command.army.get_type() != ENTITY_TYPE::ARMY) return;

	const world_entity_settlements& settlements = match.world->entities.settlements;
	if (settlements.active_count >= settlements.max_count) return;

	ui16 armyIndex = command.army.get_index();
	const world_entity_armies& armies = match.world->entities.armies;
	const world_entity_armies::comp& composition = armies.compositions[armyIndex];

	world_entity_settlements::single newSettlement = {};
	newSettlement.owner = armies.owners[armyIndex];
	newSettlement.location = armies.locations[armyIndex];
	newSettlement.population = (ui32)composition.levies + composition.archers + composition.men_at_arms
		+ composition.horsemen + composition.knights;

	match_spawn_settlement(match, newSettlement);
	match_destroy_army(match, command.army);
}

// ENTITY SET ATTACK TARGET

bool command_set_entity_attack_target_validity_check(const game_match& match, match_player_id player, const command_data_set_entity_move_target& command)
{
	// TODO(Marc): Find entity ID, determine if it can attack, and if it is controllable by this player.
	return false;
}

void command_set_entity_attack_target_apply(game_match& match, match_player_id player, const command_data_set_entity_move_target& command)
{
	// TODO(Marc): Find entity from ID in the command and set its attack target if possible.
}

// END COMMAND FUNCTION IMPLEMENTATIONS

// BEGIN CORE COMMAND HANDLING FUNCTIONS

// Checks that an input command would be valid to apply to the match over its next tick.
bool match_command_validity_check(const game_match& match, match_player_id player, const match_command_header& command)
{
	switch (command.type)
	{
		case MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET:
			return command_set_entity_move_target_validity_check(match, player, command.get_command_data<command_data_set_entity_move_target>());
		case MATCH_COMMAND_TYPE::FOUND_SETTLEMENT:
			return command_found_settlement_validity_check(match, player, command.get_command_data<command_data_found_settlement>());
		default:
			ASSERT_MSG(0, "No validity check logic associated with command type.");
			return false;
	}
}

inline void match_command_apply(game_match& match, match_player_id player, const match_command_header& command)
{
	// Route to type-specific logic.
	switch (command.type)
	{
	case MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET:
		command_set_entity_move_target_apply(match, player,
			command.get_command_data<command_data_set_entity_move_target>());
		break;
	case MATCH_COMMAND_TYPE::FOUND_SETTLEMENT:
		command_found_settlement_apply(match, player, command.get_command_data<command_data_found_settlement>());
		break;
	default:
		ASSERT_MSG(0, "No apply logic associated with command type.");
	}
}

void match_command_apply_all(game_match& match, const match_tick_commands& tick_commands)
{
	ui32 commandBufferPos = 0;

	for (ui16 sequenceIndex = 0; sequenceIndex < tick_commands.sequences_count; sequenceIndex++)
	{
		match_player_id sequencePlayerId = tick_commands.get_sequence_player_at(commandBufferPos);
		commandBufferPos += sizeof(match_player_id);

		match_command_sequence& sequence = tick_commands.get_sequence_at(commandBufferPos);
		commandBufferPos += sizeof(match_command_sequence);

		ui32 sequenceBufferPos = 0;
		for (ui16 commandIndex = 0; commandIndex < sequence.command_count; commandIndex++)
		{
			const match_command_header& commandHeader = sequence.get_sequence_at(sequenceBufferPos);
			sequenceBufferPos += sizeof(match_command_header);

			match_command_apply(match, sequencePlayerId, commandHeader);

			sequenceBufferPos += get_command_data_size(commandHeader.type);
		}
		commandBufferPos += sequenceBufferPos;
	}
}

bool match_command_sequence_output_validated(const game_match& target_match, const match_command_sequence& unvalidated,
	match_player_id player_id, command_sequence_builder& output_builder)
{
	ui32 sequenceBufferPos = 0;
	for (ui16 commandIndex = 0; commandIndex < unvalidated.command_count; commandIndex++)
	{
		const match_command_header& commandHeader = unvalidated.get_sequence_at(sequenceBufferPos);
		sequenceBufferPos += sizeof(match_command_header);

		// Route to type-specific logic.
		bool isValid = false;
		switch (commandHeader.type)
		{
		case MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET:
			isValid = command_set_entity_move_target_validity_check(target_match, player_id,
				commandHeader.get_command_data<command_data_set_entity_move_target>());
			break;
		case MATCH_COMMAND_TYPE::FOUND_SETTLEMENT:
			isValid = command_found_settlement_validity_check(target_match, player_id,
				commandHeader.get_command_data<command_data_found_settlement>());
			break;
		default:
			// Only assert if the command type IS valid but not handled.
			if ((i8)commandHeader.type >= 0 && (i8)commandHeader.type < (i8)MATCH_COMMAND_TYPE::TYPE_COUNT)
			{
				ASSERT_MSG(0, "No validity check logic associated with command type %d.", commandHeader.type);
			}

			// From there the whole unvalidated sequence becomes impossible to keep reading safely.
			// Stop process now and signal caller that the sequence couldn't be fully validated.
			return false;
		}
		sequenceBufferPos += get_command_data_size(commandHeader.type);

		// If command is valid, add it to the output sequence. Otherwise discard it.
		if (isValid)
		{
			void* validatedPayload = output_builder.push_command(commandHeader.type);
			ia_memcpy(validatedPayload, commandHeader.command_data, get_command_data_size(commandHeader.type));
		}
	}

	return true;
}

