// Entry file of the match AI component (server side). Unity-included from game_common_main.cpp.
// Hook: the host calls ai_observe_world after each match tick, then ai_output_commands for each AI player before the next one.

#include "match_ai_observe.cpp"
#include "match_ai_behaviors.cpp"

// AI players decide once per day, each on its own tick of the day so they don't all act at once.
static bool ai_is_decision_tick(const ai_observed_world_state& observed, match_player_id player)
{
	return (observed.tick + player) % GAME_PARAMETERS.ticks_per_day == 0;
}

bool ai_output_commands(const ai_observed_world_state& observed, const game_match& match, ai_player_state& ai_state,
	match_tick_commands_builder& commands_builder)
{
	if (!ai_is_decision_tick(observed, ai_state.controlled_player)) return true;

	// Decide into a scratch sequence, then push it through the validating path like any untrusted input.
	alignas(8) ui8 scratchBuffer[512];
	mem_arena scratchMem = mem_arena_create(scratchBuffer, sizeof(scratchBuffer));
	command_sequence_builder sequence = { &scratchMem };
	if (!sequence.init()) return false;

	ai_context context = { observed, ai_state.controlled_player, sequence };
	ai_decide(context);

	if (sequence._sequence_start->command_count == 0) return true;

	// Worst case size in the tick commands: player id + sequence. Pushing into a full builder would corrupt it, so skip instead.
	const mem_arena& target = *commands_builder.target_mem;
	if (target.mem_size - target.allocated_count < sequence.total_size + sizeof(match_player_id) + 8) return false;

	commands_builder.push_validated_sequence(ai_state.controlled_player, match, *sequence._sequence_start);
	return true;
}
