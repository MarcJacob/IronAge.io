// Entry file of the match AI component (server side). Unity-included from game_common_main.cpp.
// Hook: the host calls ai_update for every AI player before each match tick. The AI reads the world (result of the previous tick), never writes it.

#include "match_ai_internal.h"

#include "match_ai_world.cpp"
#include "match_ai_opinion.cpp"
#include "match_ai_strategy.cpp"
#include "match_ai_tactics_economy.cpp"
#include "match_ai_tactics_expansion.cpp"
#include "match_ai_tactics_military.cpp"

#ifdef MATCH_AI_STATS
// Test-only counters of the commands the AI emitted and the ones that passed validation, per command type.
ui32 AI_STATS_EMITTED[(ui8)MATCH_COMMAND_TYPE::TYPE_COUNT];
ui32 AI_STATS_ACCEPTED[(ui8)MATCH_COMMAND_TYPE::TYPE_COUNT];

static void ai_stats_count(const game_match& match, match_player_id player, const match_command_sequence& sequence)
{
	ui32 position = 0;
	for (ui16 index = 0; index < sequence.command_count; index++)
	{
		const match_command_header& header = sequence.get_sequence_at(position);
		position += sizeof(match_command_header) + get_command_data_size(header.type);
		AI_STATS_EMITTED[(ui8)header.type]++;
		if (match_command_validity_check(*match.world, player, header)) AI_STATS_ACCEPTED[(ui8)header.type]++;
	}
}
#endif

// One decision, layers from reactive to slow. Each layer pushes into ctx.commands until the budget is spent.
static void ai_decide(ai_context& ctx)
{
	if (ctx.settlement_count == 0) return;

	if (ai_layer_due(ctx, AI_STRATEGY_EVERY, 0)) ai_strategy_update(ctx);

	if (ai_layer_due(ctx, AI_THREAT_EVERY, 0)) ai_tactics_defend(ctx);
	ai_tactics_campaign(ctx);
	ai_tactics_assault(ctx);

	if (ai_layer_due(ctx, AI_TACTICS_EVERY, 1))
	{
		ai_tactics_taxes(ctx);
		ai_tactics_raise_armies(ctx);
		ai_tactics_disband_remnants(ctx);
		ai_tactics_buildings(ctx);
		ai_tactics_found_settlement(ctx);
		ai_tactics_roads(ctx);
		ctx.state.settlement_cursor = (ui16)((ctx.state.settlement_cursor + AI_SETTLEMENTS_PER_TACTICS) % ctx.settlement_count);
	}
}

bool ai_update(const game_match& match, ai_player_state& ai_state, match_tick_commands_builder& commands_builder)
{
	const match_world_state& world = *match.world;
	if (ai_world_age(world) % AI_DECISION_PERIOD_TICKS != ai_state.controlled_player % AI_DECISION_PERIOD_TICKS) return true;

	if (ai_state.rng_state == 0) ai_state.rng_state = 0x9E3779B9u ^ ((ui32)ai_state.controlled_player * 2654435761u + 1);

	// Decide into a scratch sequence no bigger than the room left in the tick commands, then push it through the validating path like any untrusted input.
	const mem_arena& target = *commands_builder.target_mem;
	ui64 overhead = sizeof(match_player_id) + sizeof(match_command_sequence) + 16;
	if (target.mem_size - target.allocated_count <= overhead + 32) return false;

	alignas(8) ui8 scratchBuffer[1024];
	mem_arena scratchMem = mem_arena_create(scratchBuffer, ia_min((ui64)sizeof(scratchBuffer), target.mem_size - target.allocated_count - overhead));
	command_sequence_builder sequence = { &scratchMem };
	if (!sequence.init()) return false;

	ai_context context = { world, ai_state, sequence, ai_state.controlled_player };
	ai_gather_own(context);
	ai_decide(context);
	ai_state.decision_index++;

	if (sequence._sequence_start->command_count == 0) return true;

#ifdef MATCH_AI_STATS
	ai_stats_count(match, ai_state.controlled_player, *sequence._sequence_start);
#endif

	commands_builder.push_validated_sequence(ai_state.controlled_player, match, *sequence._sequence_start);
	return true;
}
