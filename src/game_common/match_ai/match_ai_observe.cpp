// Builds the observed world state the AI players reason on: a plain copy of the few world properties they need.

#include "game_common/match/ai/match_ai.h"
#include "game_common/match/game_parameters.h"

void ai_observe_world(const match_world_state& world, ai_observed_world_state& out_observed)
{
	out_observed = {};

	out_observed.tick = world.age;
	out_observed.day = world.age / GAME_PARAMETERS.ticks_per_day;

	out_observed.player_count = ia_min(world.player_count, AI_OBSERVED_MAX_PLAYERS);
	for (ui16 playerIndex = 0; playerIndex < out_observed.player_count; playerIndex++)
	{
		out_observed.treasuries[playerIndex] = world.player_states[playerIndex].treasury;
	}

	const world_entity_settlements& settlements = world.entities->settlements;
	for (ui16 index = 0; index < settlements.max_count && out_observed.settlement_count < AI_OBSERVED_MAX_SETTLEMENTS; index++)
	{
		if (!settlements.guids[index].is_valid()) continue;

		ai_observed_settlement& observed = out_observed.settlements[out_observed.settlement_count++];
		observed.guid = settlements.guids[index];
		observed.owner = settlements.owners[index];
		observed.location = settlements.locations[index];
		observed.population = settlements.populations[index].population_total;
		observed.peasants = settlements.populations[index].peasants;
		observed.workers = settlements.populations[index].workers;
		observed.local_wealth = settlements.local_wealth[index];
		observed.tax_rate = settlements.tax_rates[index];
		observed.home_troops = settlements.populations[index].home;
		observed.buildings = settlements.buildings[index];
	}

	const world_entity_armies& armies = world.entities->armies;
	for (ui16 index = 0; index < armies.max_count && out_observed.army_count < AI_OBSERVED_MAX_ARMIES; index++)
	{
		if (!armies.guids[index].is_valid()) continue;

		ai_observed_army& observed = out_observed.armies[out_observed.army_count++];
		const army_troop_count& composition = armies.compositions[index];
		observed.guid = armies.guids[index];
		observed.owner = armies.owners[index];
		observed.location = armies.locations[index];
		observed.troops = (ui32)composition.levies + composition.men_at_arms + composition.horsemen + composition.knights;
		observed.idle = !armies.raising[index] && !armies.battles[index].is_valid() && !armies.action_targets[index].is_valid()
			&& armies.locations[index] == armies.movements[index].destination;
	}
}
