// Implementation file for various common testing functions.

#include "core.h"

#include "game_common_internals.h"
#include "game_common/match/commands.h"
#include "game_common/match/start_params.h"

// TODO(Marc): Waaaaaaaaaaaaaaaaay better test scenario system. But a static system will be enough for the bulk of early development.

#define MATCH_TEST_SCENARIO_TICKS (2000)

game_match_start_params* match_test_scenario_get_params(mem_arena& memory)
{
	game_match_start_params* params_ptr = memory.alloc<game_match_start_params>();
	ASSERT(params_ptr != nullptr);

	game_match_start_params& params = *params_ptr;
	params.tick_rate = 20;
	params.player_count = 4;
	params.world_size_regions = { 1, 1 };
	params.max_tick = MATCH_TEST_SCENARIO_TICKS;

	// Initialize player entity start locations.
	ui16 extraDataSize = 0;

	// Start settlements
	params.start_settlement_count = params.player_count;
	extraDataSize += sizeof(world_entity_settlements::single) * params.start_settlement_count;

	void* extraData = memory.alloc(extraDataSize); // Will be readable as params's extra data memory.
	ASSERT(extraData != nullptr);

	params.extra_data_size = extraDataSize;

	for (match_player_id player = 0; player < params.player_count; player++)
	{
		world_entity_settlements::single& startSettlement = params.get_settlement_start_state(player); 
		startSettlement = { 0 };

		startSettlement.owner = player;
		startSettlement.location = ia_rand_vec({ 100.f, 100.f }, params.world_size_regions * world_terrain::REGION_SIZE - vec2i{100, 100});
		startSettlement.population = 100;
		startSettlement.local_wealth = 100;
		startSettlement.tier = 1;
	}

	return &params;
}

// Procedurally generates tick commands for the given match, using the match's own memory.
match_tick_commands* build_test_scenario_commands(const game_match& match)
{
	match_tick_commands_builder commandsBuilder = {};
	commandsBuilder.target_mem = match.memory;

	if (!commandsBuilder.init()) return nullptr;

	// TODO(Marc): Rebuild proper test scenario with AI players.

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

	// TODO(Marc): Dump entity states.

	return dump_stream.dump_size;
}

