// Campaign decision-making: each tick, campaigns take over the armies joining them, follow the centroid of their member armies, end when none is close,
// and work towards their objective (gather, march, work the objective, end once it is accomplished).
// Integer math only, campaigns and armies in slot order. While marching, member armies accompany a guide army that walks to the objective.

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/entities/world_entity_armies.h"
#include "game_common/match/world/entities/world_entity_campaigns.h"
#include "game_common/match/world/entities/world_entity_common.h"
#include "game_common/match/world/world_state.h"

#include "systems_war.h"
#include "../world_sim.h"

#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_campaign.h"

#include "../world_movement.h"

// Defines a campaign objective's decision making policy while the campaign AI is actively executing it.
struct campaign_objective_def
{
	const char* name;

	// Where the campaign has to go. False if there is nowhere (e.g. its target is gone).
	bool (*get_target_loc)(const match_world_state& world, const campaign_objective& objective, world_location& out_location);

	// Whether the objective is accomplished. The campaign then ends.
	bool (*get_is_accomplished)(const match_world_state& world, match_player_id owner, const campaign_objective& objective);

	// Runs every tick while the campaign is ON_OBJECTIVE.
	void (*on_objective_logic)(match_world_state& world, entity_guid campaign);
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

// OBJECTIVES
// One row per CAMPAIGN_OBJECTIVE_TYPE, in order. NONE has no logic: the campaign only gathers.
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

// MARCHING: the guide army walks to the objective and every other member army accompanies it. Close enough to the objective, the campaign is ON_OBJECTIVE.
static void campaign_march(match_world_state& world, ui16 campaign_index, entity_guid campaign, world_location destination)
{
	world_entity_campaigns& campaigns = world.entities->campaigns;
	world_entity_armies& armies = world.entities->armies;
	world_location location = campaigns.location[campaign_index];

    ASSERT(campaigns.state[campaign_index] == CAMPAIGN_STATE_MARCHING);

	if (campaign_within_tiles(location, destination, GAME_PARAMETERS.campaign_assault_distance_tiles))
	{
		campaigns.state[campaign_index] = CAMPAIGN_STATE_ON_OBJECTIVE;
		return;
	}

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

		if (campaigns.state[campaignIndex] == CAMPAIGN_STATE_WAITING) campaigns.state[campaignIndex] = CAMPAIGN_STATE_GATHERING;
		campaigns.troop_count[campaignIndex] = (ui32)ia_min(troops, (ui64)0xFFFFFFFF);

		world_location& location = campaigns.location[campaignIndex];
		location.x = sumX / inRange;
		location.y = sumY / inRange;

		// Objective. Without one (or an unknown one) the campaign just gathers.
		const campaign_objective& objective = campaigns.objective[campaignIndex];
		const campaign_objective_def* objectiveDef = objective.type < CAMPAIGN_OBJECTIVE_TYPE_COUNT ? &CAMPAIGN_OBJECTIVE_DEFS[objective.type] : nullptr;

		if (objectiveDef == nullptr || objectiveDef->get_target_loc == nullptr) continue;

		if (objectiveDef->get_is_accomplished(world, campaigns.owner[campaignIndex], objective))
		{
			world_campaign_end_and_send_home(world, campaign);
			continue;
		}

		switch (campaigns.state[campaignIndex])
		{
		case CAMPAIGN_STATE_GATHERING:
			// Marches off once every army has arrived and stands close (the begin command skips the wait).
			if (!campaign_has_joiners(world, campaign) && members == inRange)
			{
				campaigns.state[campaignIndex] = CAMPAIGN_STATE_MARCHING;

                // Choose a guide army. @TEMP(Marc): Just pick the first army.
                campaigns.state_data[campaignIndex] = {};
                campaigns.state_data[campaignIndex].marching.guide_army = firstMember;
			}
			break;
		case CAMPAIGN_STATE_MARCHING:
		{
			world_location destination;
			if (objectiveDef->get_target_loc(world, objective, destination))
            {
                // Make sure the guide army is always set and alive (otherwise the first member takes over).
                if (!world_entity_is_valid(world, campaigns.state_data[campaignIndex].marching.guide_army))
                {
                    campaigns.state_data[campaignIndex].marching.guide_army = firstMember;
                }
                campaign_march(world, (ui16)campaignIndex, campaign, destination);
            }
            else
            {
                // Campaign objective has no location logic, so switch straight to "on objective" logic where, 
                // presumably, alternative movement logic will be provided.
                campaigns.state[campaignIndex] = CAMPAIGN_STATE_ON_OBJECTIVE;
            }
			break;
		}
		case CAMPAIGN_STATE_ON_OBJECTIVE:
            // Execute on objective logic.
            // Every objective definition must have a logic function defined, so we don't mind crashing here.
			objectiveDef->on_objective_logic(world, campaign);
			break;
		default:
			break;
		}
	}
}
