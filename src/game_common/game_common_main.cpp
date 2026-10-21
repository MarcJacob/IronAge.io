// Main implementation file for the Game Common code, which mostly covers starting and simulating a match.
// Used by client and game server to synchronize with eachother using a Lockstep strategy.

#include "core.h"

#include "core/memory.h"

#include "game_common/match/match.h"
#include "game_common/match/start_params.h"
#include "game_common/match/commands.h"

#include "game_common/match/world/world.h"
#include "game_common/match/world/world_entities.h"
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
    // Ask for some base memory for general usage, and for the world state.
 
    ui32 regionCount = params.world_size_regions.x * params.world_size_regions.y;

    // Max entity counts, as in world_state_init.
    ui64 settlementCount = (ui64)SETTLEMENTS_PER_REGION * regionCount;
    ui64 microEntityCount = (ui64)MICRO_ENTITIES_PER_TYPE_PER_REGION * regionCount;

    return MiB(32) // Base memory for general usage.
        + (ui64)regionCount * sizeof(world_terrain::region) // Terrain regions.
        + sizeof(match_world_state) + sizeof(world_terrain) + sizeof(world_entities) // World structures.
        + sizeof(world_path_store) + WORLD_PATH_STORE_SCRATCH_SIZE // Path store and its search memory.
        + settlementCount * sizeof(world_entity_settlements::single) // Settlements.
        + settlementCount * SETTLEMENT_BUILDING_TYPE_COUNT * sizeof(ui16) // Settlement building counts.
        + microEntityCount * sizeof(world_entity_armies::single) // Armies.
        + microEntityCount * sizeof(world_entity_battles::single) // Battles.
        + microEntityCount * sizeof(world_entity_workers::single) // Workers.
        + KiB(4); // Margin for the alignment padding of the ~30 allocations.
}

bool match_start(mem_arena& match_mem, time_ms start_time, game_match_start_params& params, game_match& out_match)
{
	if (params.world_size_regions.x == 0 || params.world_size_regions.y == 0)
	{
		return false; // Invalid params.
	}

	// TODO: Instead of checking total memory size of the arena, we should instead check how much it has *left* in case it already contains stuff (not recommended).
	if (match_mem.mem_size - match_mem.allocated_count < match_get_required_mem(params))
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
