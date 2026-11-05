#include "world_sim_entity_campaign.h"
#include "../world_sim.h"

entity_guid world_spawn_campaign(match_world_state& world, match_player_id owner, world_location location)
{
	world_entity_campaigns& campaigns = world.entities->campaigns;
	ui16 freeIndex = 0;
	while (freeIndex < campaigns.max_count && campaigns.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < campaigns.max_count);

	entity_guid newID = entity_guid_new(ENTITY_TYPE::CAMPAIGN, freeIndex, world.age % 0x100); // 8-bit extra field.

	world_entity_campaign_view newCampaign { campaigns, freeIndex };
	newCampaign.copy_from(world_entity_campaign {});
	newCampaign.location = location;
	newCampaign.owner = owner;
	campaigns.guids[freeIndex] = newID;
	campaigns.active_count++;

	return newID;
}

void world_campaign_end_and_send_home(match_world_state& world, entity_guid campaign)
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

void world_destroy_campaign(match_world_state& world, entity_guid campaign)
{
	ASSERT(world_entity_is_valid(world, campaign) && campaign.get_type() == ENTITY_TYPE::CAMPAIGN);
	world_entity_campaigns& campaigns = world.entities->campaigns;

	world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (armies.campaign[armyIndex] == campaign) armies.campaign[armyIndex] = INVALID_ENTITY_GUID;
	}

	world_entity_settlements& settlements = world.entities->settlements;
	for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (settlements.campaign[settlementIndex] == campaign) settlements.campaign[settlementIndex] = INVALID_ENTITY_GUID;
	}

	ui16 index = campaign.get_index();
	world_entity_campaign_view destroyedCampaign { campaigns, index };
	destroyedCampaign.copy_from(world_entity_campaign {});
	campaigns.guids[index] = INVALID_ENTITY_GUID;
	campaigns.active_count--;
}

void world_campaign_army_left(match_world_state& world, entity_guid campaign)
{
	if (!world_entity_is_valid(world, campaign) || campaign.get_type() != ENTITY_TYPE::CAMPAIGN) return;

	const world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (armies.campaign[armyIndex] == campaign) return;
	}

	world_destroy_campaign(world, campaign);
}

ui32 campaign_join_radius(const match_world_state& world, entity_guid campaign)
{
	const world_entity_armies& armies = world.entities->armies;
	ui32 members = 0;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (armies.campaign[armyIndex] == campaign) members++;
	}

	return GAME_PARAMETERS.campaign_join_base_tiles + GAME_PARAMETERS.campaign_join_per_member_tiles * members;
}

bool world_entity_query_campaign(const match_world_state& world, entity_guid entity, world_entity_campaign& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::CAMPAIGN || !world_entity_is_valid(world, entity)) return false;

	out_state.copy_from(world.entities->campaigns, entity.get_index());
	out_state.join_radius = (ui8)ia_min(campaign_join_radius(world, entity), (ui32)255);
	return true;
}

bool world_alloc_campaigns(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_campaigns& campaigns = world.entities->campaigns;

	campaigns.max_count = count;

	campaigns.guids = memory.alloc<entity_guid>(count);
	if (campaigns.guids == nullptr) return false;

	campaigns.location = memory.alloc<world_location>(count);
	if (campaigns.location == nullptr) return false;

	campaigns.owner = memory.alloc<match_player_id>(count);
	if (campaigns.owner == nullptr) return false;

	campaigns.state = memory.alloc<CAMPAIGN_STATE>(count);
	if (campaigns.state == nullptr) return false;

	campaigns.troop_count = memory.alloc<ui32>(count);
	if (campaigns.troop_count == nullptr) return false;

	campaigns.objective = memory.alloc<campaign_objective>(count);
	if (campaigns.objective == nullptr) return false;

	campaigns.waypoint = memory.alloc<world_location>(count);
	if (campaigns.waypoint == nullptr) return false;

	return true;
}
