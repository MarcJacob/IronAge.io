// Main implementation file for the Game Common code, which mostly covers starting and simulating a match.
// Used by client and game server to synchronize with eachother using a Lockstep strategy.

#include "game_common/game_match.h"

constexpr ui64 match_get_required_mem(game_match_create_params& params)
{
	return MiB(1); // For now let's just keep it static at one mibibyte.
}

bool match_create(mem_arena& match_mem, game_match_create_params& params, game_match& out_match)
{ 
	if (params.world_width < MIN_WORLD_DIM_SIZE
		|| params.world_height < MIN_WORLD_DIM_SIZE)
	{
		return false; // Invalid params.
	}

	params.tick_rate = MATCH_TICK_RATE;

	// TODO: Instead of checking total memory size of the arena, we should instead check how much it has *left* in case it already contains stuff (not recommended).
	if (match_mem.mem_size < match_get_required_mem(params))
	{
		return false; // Not enough memory.
	}

	out_match = {};
	out_match.start_params = params;
	out_match.memory = &match_mem;

	return true;
}

void match_start(game_match& match, ui64 start_time_epoch)
{
	match.start_time = start_time_epoch;

	match.world_state = match.memory->alloc<match_world_state>();
	*match.world_state = {};
}

// Advances time in a match's world simulation. Requires the aggregated inputs / commands to apply into the tick.
void match_tick(game_match& match, const match_tick_commands& input)
{
	match.tick++;

	match_world_state& world = *match.world_state;
	world.entity_target_x = input.target_x;
	world.entity_target_y = input.target_y;

	// Manhattan travel: one tile per tick, independently on each axis.
	if (world.entity_loc_x < world.entity_target_x) world.entity_loc_x++;
	else if (world.entity_loc_x > world.entity_target_x) world.entity_loc_x--;

	if (world.entity_loc_y < world.entity_target_y) world.entity_loc_y++;
	else if (world.entity_loc_y > world.entity_target_y) world.entity_loc_y--;
}

// TODO(Marc): Waaaaaaaaaaaaaaaaay better test scenario system. But a static system will be enough for the bulk of early development.

#define MATCH_TEST_SCENARIO_TICKS (200)

game_match_create_params match_test_scenario_get_params()
{
	game_match_create_params params = {
		.world_width = 1024,
		.world_height = 1024
	};
	
	return params;
}

game_match* match_run_test_scenario(mem_arena& match_mem)
{
	game_match_create_params params = match_test_scenario_get_params();
	game_match* match = match_mem.alloc<game_match>();
	ASSERT(match != nullptr);

	if (!match_create(match_mem, params, *match))
	{
		return nullptr;
	}

	// Start and run the required number of ticks over the match.
	match_start(*match, 0);
	while (match->tick < MATCH_TEST_SCENARIO_TICKS)
	{
		match_tick_commands commands = {};
		commands.target_x = match->tick * 200 % 1024;
		commands.target_y = match->tick * 100 % 512 * 2;
		match_tick(*match, commands);
	}

	return match;
}

ui64 match_dump_gamestate(game_match& match, match_dump_stream& dump_stream)
{
	dump_stream.dump_size = 0;

	dump_stream.dump(match.start_time);
	dump_stream.dump(match.tick);
	dump_stream.dump(match.start_params.world_width);
	dump_stream.dump(match.start_params.world_height);

	match_world_state& world = *match.world_state;
	dump_stream.dump(world.entity_loc_x);
	dump_stream.dump(world.entity_loc_y);
	dump_stream.dump(world.entity_target_x);
	dump_stream.dump(world.entity_target_y);

	return dump_stream.dump_size;
}
