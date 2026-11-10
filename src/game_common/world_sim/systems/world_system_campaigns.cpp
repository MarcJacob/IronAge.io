// Campaign decision-making: each tick, campaigns take over the armies joining them, follow the centroid of their member armies, end when none is close,
// and act on their decision: pursue an objective of the board (travel to it, then run its logic) or WAIT (armies stand where they are, gathering men and supplies).
// The decision is the best-ranked option (world_campaign_rank_objectives), chosen again every campaign_decision_period_ticks. A campaign never ends by accomplishing something:
// it ends when disbanded or when it has no members.
// Integer math only, campaigns and armies in slot order. While travelling, member armies accompany a guide army that walks to the objective.
// The campaign's state is derived from its decision, for display: GATHERING = WAIT, MARCHING = travelling to the objective, ON_OBJECTIVE = running its logic.

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/entities/world_entity_armies.h"
#include "game_common/match/world/entities/world_entity_campaigns.h"
#include "game_common/match/world/entities/world_entity_campaign_objectives.h"
#include "game_common/match/world/entities/world_entity_common.h"
#include "game_common/match/world/world_state.h"

#include "systems_war.h"
#include "../world_sim.h"

#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_campaign.h"
#include "../entities/world_sim_entity_engagement.h"

#include "../world_movement.h"

// HELPERS

static bool campaign_within_tiles(world_location a, world_location b, ui32 tiles)
{
	return vec2_dist_squared_i<ui32>(a, b) <= tiles * tiles;
}

// Armies with a JOIN_CAMPAIGN order targeting the campaign, within the radius of its location, become members: they stand where they are.
// Raising armies are not taken over.
static void campaign_take_over_joiners(match_world_state& world, entity_guid campaign, ui32 radius)
{
	world_entity_armies& armies = world.entities->armies;
	world_location campaignLocation = world.entities->campaigns.location[campaign.get_index()];

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || armies.raising[armyIndex] || armies.campaign[armyIndex].is_valid()) continue;
		if (armies.order[armyIndex].type != ARMY_ORDER_JOIN_CAMPAIGN || !(armies.order[armyIndex].target == campaign)) continue;
		if (!campaign_within_tiles(campaignLocation, armies.location[armyIndex], radius)) continue;

		armies.campaign[armyIndex] = campaign;
		army_order_station(armies, (ui16)armyIndex, armies.location[armyIndex], true);
		world_movement_stop(armies.movement[armyIndex], armies.location[armyIndex]);
	}
}

// A settlement of the campaign whose campaign army is gone (destroyed in battle or otherwise) raises a new one, through the same path as joining (world_settlement_join_campaign).
// The raise happens at least one tick after the loss is noticed (see below). Quietly retried every tick while it cannot (no treasury, no men, no army slot...).
// A settlement that changed owner leaves the campaign.
static void campaign_replace_lost_armies(match_world_state& world, entity_guid campaign)
{
	world_entity_settlements& settlements = world.entities->settlements;
	for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid() || !(settlements.campaign[settlementIndex] == campaign)) continue;
		if (world_entity_is_valid(world, settlements.campaign_army[settlementIndex])) continue;

		if (settlements.owner[settlementIndex] != world.entities->campaigns.owner[campaign.get_index()])
		{
			settlements.campaign[settlementIndex] = INVALID_ENTITY_GUID;
			continue;
		}

		// The loss is only noticed here: a destroyed army leaves its (now invalid) guid in campaign_army. The first tick that sees it resets the pointer to INVALID and raises nothing;
		// the replacement is raised on a later tick, whatever the order of the systems and of the destruction within the tick. (A settlement of a campaign has INVALID there only in that state.)
		if (!(settlements.campaign_army[settlementIndex] == INVALID_ENTITY_GUID))
		{
			settlements.campaign_army[settlementIndex] = INVALID_ENTITY_GUID;
			continue;
		}
		world_settlement_join_campaign(world, settlementIndex, campaign);
	}
}

// Whether some army is still on its way to join the campaign.
static bool campaign_has_joiners(const match_world_state& world, entity_guid campaign)
{
	const world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || armies.campaign[armyIndex].is_valid()) continue;
		if (armies.order[armyIndex].type == ARMY_ORDER_JOIN_CAMPAIGN && armies.order[armyIndex].target == campaign) return true;
	}
	return false;
}

// The campaign now pursues the objective (INVALID = WAIT): the old objective has one campaign less, the new one one more.
static void campaign_set_current_objective(match_world_state& world, ui16 campaign_index, entity_guid objective)
{
	world_entity_campaigns& campaigns = world.entities->campaigns;
	world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;

	entity_guid old = campaigns.current_objective[campaign_index];
	if (world_entity_is_valid(world, old) && objectives.committed[old.get_index()] > 0) objectives.committed[old.get_index()]--;

	campaigns.current_objective[campaign_index] = objective;
	if (world_entity_is_valid(world, objective)) objectives.committed[objective.get_index()]++;

	campaigns.state_data[campaign_index] = {}; // The guide army is chosen again.
}

// WAIT: every member army stands where it is.
static void campaign_stand_still(match_world_state& world, entity_guid campaign)
{
	world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!(armies.campaign[armyIndex] == campaign)) continue;

		army_order_station(armies, (ui16)armyIndex, armies.location[armyIndex], true);
		world_movement_stop(armies.movement[armyIndex], armies.location[armyIndex]);
	}
}

// TRAVELLING: the guide army walks to the objective and every other member army accompanies it.
static void campaign_march(match_world_state& world, ui16 campaign_index, entity_guid campaign, world_location destination)
{
	world_entity_campaigns& campaigns = world.entities->campaigns;
	world_entity_armies& armies = world.entities->armies;

	// Set all armies to accompany the guide army.
	for (ui16 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!(armies.campaign[armyIndex] == campaign))
			continue;
		if (armies.order[armyIndex].type == ARMY_ORDER_ACCOMPANY && armies.order[armyIndex].target == campaigns.state_data[campaign_index].marching.guide_army)
			continue;

		if (armies.guids[armyIndex]._value != campaigns.state_data[campaign_index].marching.guide_army._value)
		{
			army_order_accompany(armies, armyIndex, campaigns.state_data[campaign_index].marching.guide_army);
		}
		else
		{
			// Guide army goes to the destination.
			army_order_station(armies, armyIndex, destination, true);
		}
	}
}

// SYSTEM

void world_system_campaigns(match_world_state& world)
{
	world_entity_campaigns& campaigns = world.entities->campaigns;
	world_entity_armies& armies = world.entities->armies;

	ui64 decisionPeriod = GAME_PARAMETERS.campaign_ai.campaign_decision_period_ticks > 0 ? GAME_PARAMETERS.campaign_ai.campaign_decision_period_ticks : 1;

	for (ui32 campaignIndex = 0; campaignIndex < campaigns.max_count; campaignIndex++)
	{
		entity_guid campaign = campaigns.guids[campaignIndex];
		if (!campaign.is_valid()) continue;

		campaign_take_over_joiners(world, campaign, campaign_join_radius(world, campaign));

		// Member armies: all count in the troop total, those within the radius (it grew with the new members) make up the centroid.
		ui32 radius = campaign_join_radius(world, campaign);
		ui64 troops = 0;

		ui64 sumX = 0, sumY = 0;

		ui32 members = 0;
		ui32 inRange = 0;

		// @TEMP(Marc): Cache first member army to be used as guide army when marching.
		entity_guid firstMember {};

		for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			if (!(armies.campaign[armyIndex] == campaign)) continue;

			if (firstMember._value == 0) firstMember = armies.guids[armyIndex];

			members++;
			troops += army_total_manpower(armies.composition[armyIndex]);
			if (!campaign_within_tiles(campaigns.location[campaignIndex], armies.location[armyIndex], radius)) continue;

			sumX += armies.location[armyIndex].x;
			sumY += armies.location[armyIndex].y;
			inRange++;
		}

		if (inRange == 0)
		{
			// A campaign still waiting for its first army stays where it was founded, as long as an army is on its way (or being raised) to join it: if that army is gone
			// (raising stopped before any man), the campaign is over, otherwise it would stay forever and keep its settlement from joining anything.
			// Otherwise no army is close enough: the campaign is over.
			if (campaigns.state[campaignIndex] != CAMPAIGN_STATE_WAITING || (members == 0 && !campaign_has_joiners(world, campaign))) world_destroy_campaign(world, campaign);
			continue;
		}

		// The campaign has members: its settlements replace the armies they lost.
		campaign_replace_lost_armies(world, campaign);

		campaigns.troop_count[campaignIndex] = (ui32)ia_min(troops, (ui64)0xFFFFFFFF);

		world_location& location = campaigns.location[campaignIndex];
		location.x = sumX / inRange;
		location.y = sumY / inRange;

		// Decision. An objective that is gone (despawned, its counter with it) is forgotten at once and a new decision is made; otherwise on this campaign's turn of the period.
		bool objectiveChanged = false;
		bool objectiveLost = campaigns.current_objective[campaignIndex].is_valid() && !world_entity_is_valid(world, campaigns.current_objective[campaignIndex]);
		if (objectiveLost) campaigns.current_objective[campaignIndex] = INVALID_ENTITY_GUID;

		if (objectiveLost || (world.age + campaignIndex) % decisionPeriod == 0)
		{
			campaign_decision decision = world_campaign_rank_objectives(world, campaign, campaigns.current_objective[campaignIndex]);
			if (!(decision.objective == campaigns.current_objective[campaignIndex]))
			{
				campaign_set_current_objective(world, (ui16)campaignIndex, decision.objective);
				objectiveChanged = true;
				if (!decision.objective.is_valid()) campaign_stand_still(world, campaign);
			}
		}

		// Action.
		entity_guid current = campaigns.current_objective[campaignIndex];
		if (!current.is_valid())
		{
			campaigns.state[campaignIndex] = CAMPAIGN_STATE_GATHERING;
			continue;
		}

		world_location destination = world_entity_get_location(world, current);

		// Once on the objective the campaign stays on it (its armies may be fighting there) until it chooses something else.
		bool onObjective = campaign_within_tiles(location, destination, campaign_objective_range_tiles(world, current))
			|| (campaigns.state[campaignIndex] == CAMPAIGN_STATE_ON_OBJECTIVE && !objectiveChanged);

		if (onObjective)
		{
			campaigns.state[campaignIndex] = CAMPAIGN_STATE_ON_OBJECTIVE;
			campaign_objective_run_logic(world, campaign, current);
		}
		else
		{
			campaigns.state[campaignIndex] = CAMPAIGN_STATE_MARCHING;

			// Make sure the guide army is always set and alive (otherwise the first member takes over).
			if (!world_entity_is_valid(world, campaigns.state_data[campaignIndex].marching.guide_army)) campaigns.state_data[campaignIndex].marching.guide_army = firstMember;
			campaign_march(world, (ui16)campaignIndex, campaign, destination);
		}
	}
}
