// Campaign decision-making: each tick, campaigns take over the armies joining them, follow the centroid of their member armies, end when none is close,
// and work towards their objective (gather, march, work the objective, end once it is accomplished).
// Integer math only, campaigns and armies in slot order. The world's random generator (deterministic) spreads the armies around waypoints.

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world_state.h"

#include "systems_war.h"
#include "../world_sim.h"

#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_campaign.h"

#include "../world_movement.h"

// OBJECTIVES
// One row per CAMPAIGN_OBJECTIVE_TYPE, in order. NONE has no logic: the campaign only gathers.

struct campaign_objective_def
{
	const char* name;

	// Where the campaign has to go. False if there is nowhere (e.g. its target is gone).
	bool (*location)(const match_world_state& world, const campaign_objective& objective, world_location& out_location);

	// Whether the objective is accomplished. The campaign then ends.
	bool (*accomplished)(const match_world_state& world, match_player_id owner, const campaign_objective& objective);

	// Runs every tick while the campaign is ON_OBJECTIVE.
	void (*on_objective)(match_world_state& world, entity_guid campaign);
};

// CONQUER_SETTLEMENT

// The target is a settlement that is still hostile or neutral to the owner.
static bool conquer_target_is_unconquered(const match_world_state& world, match_player_id owner, const campaign_objective& objective)
{
	return world_entity_is_valid(world, objective.target) && objective.target.get_type() == ENTITY_TYPE::SETTLEMENT
		&& (world_entity_is_hostile(world, objective.target, owner) || world_entity_is_neutral(world, objective.target, owner));
}

static bool conquer_location(const match_world_state& world, const campaign_objective& objective, world_location& out_location)
{
	if (!world_entity_is_valid(world, objective.target) || objective.target.get_type() != ENTITY_TYPE::SETTLEMENT) return false;
	out_location = world.entities->settlements.location[objective.target.get_index()];
	return true;
}

static bool conquer_accomplished(const match_world_state& world, match_player_id owner, const campaign_objective& objective)
{
	return !conquer_target_is_unconquered(world, owner, objective);
}

// Every member army attacks the settlement (the siege and assault logic of the standing orders does the rest).
static void conquer_on_objective(match_world_state& world, entity_guid campaign)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid target = world.entities->campaigns.objective[campaign.get_index()].target;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!(armies.campaign[armyIndex] == campaign)) continue;
		if (armies.order[armyIndex].type == ARMY_ORDER_ATTACK && armies.order[armyIndex].target == target) continue;

		army_order_attack(armies, (ui16)armyIndex, target);
	}
}

static const campaign_objective_def CAMPAIGN_OBJECTIVE_DEFS[CAMPAIGN_OBJECTIVE_TYPE_COUNT] =
{
	{ "NONE", nullptr, nullptr, nullptr },
	{ "CONQUER_SETTLEMENT", conquer_location, conquer_accomplished, conquer_on_objective },
};

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
		army_order_station(armies, (ui16)armyIndex, armies.location[armyIndex]);
		world_movement_stop(armies.movement[armyIndex], armies.location[armyIndex]);
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

// Moves a coordinate towards a target by at most the cap.
static ui16 campaign_step_towards(ui16 from, ui32 to, ui32 cap)
{
	i32 delta = (i32)to - (i32)from;
	delta = ia_max(ia_min(delta, (i32)cap), -(i32)cap);
	return (ui16)((i32)from + delta);
}

// A random land tile within the spread of the center (the center itself if none is found).
static world_location campaign_spread_spot(match_world_state& world, world_location center, ui32 spread)
{
	const world_dimensions& size = world.terrain->size_tiles;
	ui32 span = 2 * spread + 1;

	for (ui8 attempt = 0; attempt < 8; attempt++)
	{
		i32 x = (i32)center.x + (i32)(world.rand_gen.next<ui32>() % span) - (i32)spread;
		i32 y = (i32)center.y + (i32)(world.rand_gen.next<ui32>() % span) - (i32)spread;
		if (x < 0 || y < 0 || x >= (i32)size.x || y >= (i32)size.y) continue;

		world_location spot = { (ui16)x, (ui16)y };
		if (world_tile_is_land(*world.terrain, spot)) return spot;
	}
	return center;
}

// The campaign is over with its objective accomplished: its armies go home and it is destroyed.
static void campaign_end_and_send_home(match_world_state& world, entity_guid campaign)
{
	world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!(armies.campaign[armyIndex] == campaign)) continue;

		armies.order[armyIndex] = {};
		armies.order[armyIndex].type = ARMY_ORDER_GO_HOME;
	}
	world_destroy_campaign(world, campaign);
}

// MARCHING: the campaign goes on in steps. Each time it gets close to the last waypoint, a new one is picked a step ahead towards the objective,
// and every member army is sent to its own random spot around it. Close enough to the objective, the campaign is ON_OBJECTIVE.
static void campaign_march(match_world_state& world, ui16 campaign_index, entity_guid campaign, world_location destination)
{
	world_entity_campaigns& campaigns = world.entities->campaigns;
	world_entity_armies& armies = world.entities->armies;
	world_location location = campaigns.location[campaign_index];

	if (campaign_within_tiles(location, destination, GAME_PARAMETERS.campaign_assault_distance_tiles))
	{
		campaigns.state[campaign_index] = CAMPAIGN_STATE_ON_OBJECTIVE;
		return;
	}

	if (!campaign_within_tiles(location, campaigns.waypoint[campaign_index], GAME_PARAMETERS.campaign_march_arrive_tiles)) return; // Still walking to it.

	world_location waypoint = {
		campaign_step_towards(location.x, destination.x, GAME_PARAMETERS.campaign_march_step_tiles),
		campaign_step_towards(location.y, destination.y, GAME_PARAMETERS.campaign_march_step_tiles),
	};
	campaigns.waypoint[campaign_index] = waypoint;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!(armies.campaign[armyIndex] == campaign)) continue;

		army_order_station(armies, (ui16)armyIndex, campaign_spread_spot(world, waypoint, GAME_PARAMETERS.campaign_spread_tiles));
	}
}

// SYSTEM

void world_system_campaigns(match_world_state& world)
{
	world_entity_campaigns& campaigns = world.entities->campaigns;
	world_entity_armies& armies = world.entities->armies;

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
		for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			if (!(armies.campaign[armyIndex] == campaign)) continue;

			members++;
			troops += army_total_manpower(armies.composition[armyIndex]);
			if (!campaign_within_tiles(campaigns.location[campaignIndex], armies.location[armyIndex], radius)) continue;

			sumX += armies.location[armyIndex].x;
			sumY += armies.location[armyIndex].y;
			inRange++;
		}

		if (inRange == 0)
		{
			// A campaign still waiting for its first army stays where it was founded. Otherwise no army is close enough: the campaign is over.
			if (campaigns.state[campaignIndex] != CAMPAIGN_STATE_WAITING) world_destroy_campaign(world, campaign);
			continue;
		}

		if (campaigns.state[campaignIndex] == CAMPAIGN_STATE_WAITING) campaigns.state[campaignIndex] = CAMPAIGN_STATE_GATHERING;
		campaigns.troop_count[campaignIndex] = (ui32)ia_min(troops, (ui64)0xFFFFFFFF);

		world_location& location = campaigns.location[campaignIndex];
		location.x = campaign_step_towards(location.x, (ui32)(sumX / inRange), GAME_PARAMETERS.campaign_move_cap_tiles);
		location.y = campaign_step_towards(location.y, (ui32)(sumY / inRange), GAME_PARAMETERS.campaign_move_cap_tiles);

		// Objective. Without one (or an unknown one) the campaign just gathers.
		const campaign_objective& objective = campaigns.objective[campaignIndex];
		const campaign_objective_def* def = objective.type < CAMPAIGN_OBJECTIVE_TYPE_COUNT ? &CAMPAIGN_OBJECTIVE_DEFS[objective.type] : nullptr;
		if (def == nullptr || def->location == nullptr) continue;

		if (def->accomplished(world, campaigns.owner[campaignIndex], objective))
		{
			campaign_end_and_send_home(world, campaign);
			continue;
		}

		switch (campaigns.state[campaignIndex])
		{
		case CAMPAIGN_STATE_GATHERING:
			// Marches off once every army has arrived and stands close (the begin command skips the wait).
			if (!campaign_has_joiners(world, campaign) && members == inRange)
			{
				campaigns.state[campaignIndex] = CAMPAIGN_STATE_MARCHING;
				campaigns.waypoint[campaignIndex] = campaigns.location[campaignIndex];
			}
			break;
		case CAMPAIGN_STATE_MARCHING:
		{
			world_location destination;
			if (def->location(world, objective, destination)) campaign_march(world, (ui16)campaignIndex, campaign, destination);
			break;
		}
		case CAMPAIGN_STATE_ON_OBJECTIVE:
			def->on_objective(world, campaign);
			break;
		default:
			break;
		}
	}
}
