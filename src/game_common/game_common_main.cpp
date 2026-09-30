// Main implementation file for the Game Common code, which mostly covers starting and simulating a match.
// Used by client and game server to synchronize with eachother using a Lockstep strategy.

#include "core.h"

#include "game_common/game_match.h"
#include "game_common/game_commands.h"

// Unity compile AI player system.
#include "AI_logic.cpp"

// BEGIN COMMAND FUNCTIONS

// Example of command sanity check function.
bool command_set_entity_move_target_validity_check(const game_match& match, match_player_id player, const command_payload_set_entity_move_target& command)
{
	// Ownership: Entity ID == Player ID.
	if (player != command.target_entity) return false; // Can only be controlled by owning player.
	if (command.target_entity >= match.world_state.entity_count) return false; // Must be valid target entity.

	return true;
}

// Example of a command apply function.
void command_set_entity_move_target_apply(game_match& match, match_player_id player, const command_payload_set_entity_move_target& command)
{
	// Sets target entity's location.

	match_world_state::entity& targetEntity = match.world_state.entity_states[command.target_entity];
	targetEntity.target_location = command.new_target;
}

// END COMMAND FUNCTIONS

constexpr ui64 match_get_required_mem(game_match_start_params& params)
{
	return MiB(1); // For now let's just keep it static at one mibibyte.
}

bool match_start(mem_arena& match_mem, time_ms start_time, game_match_start_params& params, game_match& out_match)
{
	if (params.world_dimensions.x < MIN_WORLD_DIM_SIZE
		|| params.world_dimensions.y < MIN_WORLD_DIM_SIZE)
	{
		return false; // Invalid params.
	}

	// TODO: Instead of checking total memory size of the arena, we should instead check how much it has *left* in case it already contains stuff (not recommended).
	if (match_mem.mem_size < match_get_required_mem(params))
	{
		return false; // Not enough memory.
	}

	out_match = {};
	out_match.start_params = &params;
	out_match.memory = &match_mem;

	out_match.start_time = start_time;

	// Initialize match world state.
	out_match.world_state = {};

	// Allocate player entities.
	out_match.world_state.entity_count = params.player_count;
	out_match.world_state.entity_states = out_match.memory->alloc<match_world_state::entity>(params.player_count);
	ASSERT(out_match.world_state.entity_states != nullptr);

	// Apply entity start positions.
	for (ui8 startEntityIndex = 0; startEntityIndex < params.player_count; startEntityIndex++)
	{
		out_match.world_state.entity_states[startEntityIndex].location = params.get_player_start_pos(startEntityIndex);
	}

	return true;
}

// Checks that an input command would be valid to apply to the match over its next tick.
bool match_command_validity_check(game_match& match, match_player_id player, const match_command_header& command)
{
	switch (command.type)
	{
		case MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET:
			return command_set_entity_move_target_validity_check(match, player, command.get_command_data<command_payload_set_entity_move_target>());
		default:
			ASSERT_MSG(0, "No validity check logic associated with command type.");
			return false;
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
				commandHeader.get_command_data<command_payload_set_entity_move_target>());
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

// Advances time in a match's world simulation. Requires the aggregated inputs / commands to apply into the tick.
void match_tick(game_match& match, const match_tick_commands& commands)
{
	match.tick++;

	match_world_state& world = match.world_state;

	// Apply inputs.
	ui32 commandBufferPos = 0;

	for (ui16 sequenceIndex = 0; sequenceIndex < commands.sequences_count; sequenceIndex++)
	{
		match_player_id sequencePlayerId = commands.get_sequence_player_at(commandBufferPos);
		commandBufferPos += sizeof(match_player_id);

		match_command_sequence& sequence = commands.get_sequence_at(commandBufferPos);
		commandBufferPos += sizeof(match_command_sequence);

		ui32 sequenceBufferPos = 0;
		for (ui16 commandIndex = 0; commandIndex < sequence.command_count; commandIndex++)
		{
			const match_command_header& commandHeader = sequence.get_sequence_at(sequenceBufferPos);
			sequenceBufferPos += sizeof(match_command_header);

			// Route to type-specific logic.
			switch (commandHeader.type)
			{
			case MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET:
				command_set_entity_move_target_apply(match, sequencePlayerId,
					commandHeader.get_command_data<command_payload_set_entity_move_target>());
				break;
			default:
				ASSERT_MSG(0, "No apply logic associated with command type.");
			}

			sequenceBufferPos += get_command_data_size(commandHeader.type);
		}
		commandBufferPos += sequenceBufferPos;
	}

	// Process entity behavior.
	for (entity_id entityID = 0; entityID < world.entity_count; entityID++)
	{
		match_world_state::entity& entity = world.entity_states[entityID];

		// Move to target location.

		// Manhattan travel: one tile per tick, independently on each axis.
		if (entity.location.x < entity.target_location.x) entity.location.x++;
		else if (entity.location.x > entity.target_location.x) entity.location.x--;

		if (entity.location.y < entity.target_location.y) entity.location.y++;
		else if (entity.location.y > entity.target_location.y) entity.location.y--;
	}

}

// TODO(Marc): Waaaaaaaaaaaaaaaaay better test scenario system. But a static system will be enough for the bulk of early development.

#define MATCH_TEST_SCENARIO_TICKS (2000)

game_match_start_params* match_test_scenario_get_params(mem_arena& memory)
{
	game_match_start_params* params_ptr = memory.alloc<game_match_start_params>();
	ASSERT(params_ptr != nullptr);

	game_match_start_params& params = *params_ptr;
	params.tick_rate = 20;
	params.player_count = 10;
	params.world_dimensions = { 1024, 1024 };
	params.max_tick = MATCH_TEST_SCENARIO_TICKS;

	// Initialize player entity start locations.
	ui16 extraDataSize = sizeof(world_location) * params.player_count;

	void* extraData = memory.alloc(extraDataSize); // Will be readable as params's extra data memory.
	ASSERT(extraData != nullptr);

	params.extra_data_size = extraDataSize;

	for (match_player_id player = 0; player < params.player_count; player++)
	{
		params.get_player_start_pos(player) = ia_rand_vec({0, 0}, params.world_dimensions);
	}

	params.extra_data_size = extraDataSize;
	return &params;
}

// Procedurally generates tick commands for the given match, using the match's own memory.
match_tick_commands* build_test_scenario_commands(const game_match& match)
{
	match_tick_commands_builder commandsBuilder = {};
	commandsBuilder.target_mem = match.memory;

	if (!commandsBuilder.init()) return nullptr;

	if (match.tick == 0)
	{
		// For each player in the match, have them move their entity to a point on the map.
		for (match_player_id playerID = 0; playerID < match.start_params->player_count; playerID++)
		{
			// Push new sequence for this player.
			if (!commandsBuilder.push_new_sequence(playerID)) return nullptr;

			// Output a single command, to move the entity to a target location that depends on the player's ID.
			world_location targetLoc = {
				(ui16)((playerID + match.tick) * 2000 / match.start_params->world_dimensions.x * 300 % match.start_params->world_dimensions.x),
				(ui16)((playerID + match.tick) * 5000 % match.start_params->world_dimensions.y) };

			auto payload = commandsBuilder.push_command<command_payload_set_entity_move_target>(MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET);
			if (payload == nullptr) return nullptr;

			payload->target_entity = playerID;
			payload->new_target = targetLoc;
		}
	}

	// Extract constructed tick commands.
	return commandsBuilder._tick_commands_start;
}

game_match* match_run_test_scenario(mem_arena& match_mem)
{
	game_match_start_params* params = match_test_scenario_get_params(match_mem);
	ASSERT(params != nullptr);

	game_match* match = match_mem.alloc<game_match>();
	ASSERT(match != nullptr);

	// Start and run the required number of ticks over the match.
	if (!match_start(match_mem, 0, *params, *match))
	{
		return nullptr;
	}

	while (match->tick < MATCH_TEST_SCENARIO_TICKS)
	{
		ui64 input_memory_start = match->memory->allocated_count;

		match_tick_commands* commands = build_test_scenario_commands(*match);
		ASSERT(commands != nullptr);

		match_tick(*match, *commands);

		// "Free" the input memory by setting the allocated count back.
		match->memory->allocated_count = input_memory_start;
	}

	return match;
}

ui64 match_dump_gamestate(game_match& match, match_dump_stream& dump_stream)
{
	dump_stream.dump_size = 0;

	dump_stream.dump(match.start_time);
	dump_stream.dump(match.tick);
	dump_stream.dump(*match.start_params);

	match_world_state& world = match.world_state;

	dump_stream.dump(world.entity_count);
	for (entity_id entity = 0; entity < world.entity_count; entity++)
	{
		dump_stream.dump(world.entity_states[entity]);
	}

	return dump_stream.dump_size;
}
