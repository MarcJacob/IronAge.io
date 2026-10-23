// Army economy systems: armies cost the treasury of their owner every day.
// The money paid from the treasury either goes to the settlement where the troops are located, or into the army in the army's reserve.
// Armies consume their reserve wealth at the same rate as their upkeep takes, meaning higher quality troops consume more, broadly.
// When armies run out of reserves, troops start deserting, in which case they go home.
// Armies can only be paid when inside friendly territory, meaning they must be in friendly territory regularly, or find *alternative* means of financing themselves...

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world.h"
#include "../entities/world_sim_entity_army.h"

#include "../world_sim.h"

// Runs daily. The treasury pays as much as it can (never below 0, no debt, no other penalty).
void world_system_armies_upkeep(match_world_state& world)
{
	if (!world_is_daily_tick(world)) return;

	world_entity_armies& armies = world.entities->armies;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;

		match_player_id owner = armies.owners[armyIndex];
		if (owner >= world.player_count) continue;

		const army_troop_count& troops = armies.compositions[armyIndex];
		ui64 cost = ((ui64)troops.levies * GAME_PARAMETERS.upkeep_levy_per_mille) / 1000
			+ (ui64)troops.men_at_arms * GAME_PARAMETERS.upkeep_men_at_arms
			+ (ui64)troops.horsemen * GAME_PARAMETERS.upkeep_horseman
			+ (ui64)troops.knights * GAME_PARAMETERS.upkeep_knight;

		world_player_state& player = world.player_states[owner];
		ui32 paid = (ui32)ia_min(cost, (ui64)player.treasury);
		player.treasury -= paid;
		player.expenses += paid;
	}
}

void world_system_local_troops_upkeep(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;
}
