// Main implementation file for the Game Common code, which mostly covers starting and simulating a match.
// Used by client and game server to synchronize with eachother using a Lockstep strategy.

#include "core.h"

#include "core/memory.h"

#include "game_common/match/match.h"
#include "game_common/match/start_params.h"
#include "game_common/match/commands.h"

#include "world_sim/world_sim.h"

// Unity compile game common components.
#include "world_sim/world_sim_main.cpp"
#include "match_commands.cpp"
#include "match_AI_logic.cpp"
#include "game_common_tests.cpp"

// Forward declare apply-all function implemented in commands components.
void match_command_apply_all(match_world_state& world, const match_tick_commands& tick_commands);

constexpr ui64 match_get_required_mem(game_match_start_params& params)
{
	return MiB(32); // For now let's just keep it static at 32 mibibyte.
}

bool match_start(mem_arena& match_mem, time_ms start_time, game_match_start_params& params, game_match& out_match)
{
	if (params.world_size_regions.x == 0 || params.world_size_regions.y == 0)
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

    // Initialize match world state.
    out_match.world = world_state_init(match_mem, params);
    if (out_match.world == nullptr) return false;

	out_match.start_time = start_time;
	return true;
}

// Advances time in a match's world simulation. Requires the aggregated inputs / commands to apply into the tick.
void match_tick(game_match& match, const match_tick_commands& commands)
{
	match.tick++;

	// Start by applying all input commands sequentially.
	match_command_apply_all(*match.world, commands);

	// Run world behavior.
	match_world_state& world = *match.world;
    world_state_tick(world);
}
