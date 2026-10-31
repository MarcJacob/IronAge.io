#include "world_sim_entity_engagement.h"
#include "world_sim_entity_army.h"

entity_guid world_spawn_engagement(match_world_state& world, const world_entity_engagements::state& start_state)
{
	world_entity_engagements& engagements = world.entities->engagements;
	ui16 freeIndex = 0;
	while (freeIndex < engagements.max_count && engagements.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < engagements.max_count);

	entity_guid newID = entity_guid_new(ENTITY_TYPE::ENGAGEMENT, freeIndex, world.age % 0x100); // 8-bit extra field.

	world_entity_engagements::view newEngagement { engagements, freeIndex };
	newEngagement.copy_from(start_state);
	newEngagement.data.phase_tick = world.age;
	engagements.guids[freeIndex] = newID;
	engagements.active_count++;

	// A siege is recorded on the besieged settlement (cleared in world_destroy_engagement).
	if (start_state.kind == world_entity_engagements::KIND_SIEGE)
	{
		ASSERT(start_state.main_target.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, start_state.main_target));
		world.entities->settlements.sieges[start_state.main_target.get_index()] = newID;
	}

	return newID;
}

void world_destroy_engagement(match_world_state& world, entity_guid engagement)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	world_entity_engagements& engagements = world.entities->engagements;

	ui16 index = engagement.get_index();

	// Clear the besieged settlement's siege (it may be gone already, or under another siege by now).
	const world_entity_engagements::state& ended = engagements.states[index];
	if (ended.kind == world_entity_engagements::KIND_SIEGE && ended.main_target.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, ended.main_target))
	{
		entity_guid& siege = world.entities->settlements.sieges[ended.main_target.get_index()];
		if (siege == engagement) siege = INVALID_ENTITY_GUID;
	}

	world_entity_engagement_view destroyedEngagement { engagements, index };
	destroyedEngagement.copy_from(world_entity_engagement {});
	engagements.guids[index] = INVALID_ENTITY_GUID;
	engagements.active_count--;
}

// Total men of the armies of one owner on one side of a engagement.
static ui32 engagement_owner_men(const match_world_state& world, entity_guid engagement, ui8 side, match_player_id owner)
{
	const world_entity_armies& armies = world.entities->armies;
	ui32 total = 0;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (armies.guids[armyIndex].is_valid() && armies.engagements[armyIndex] == engagement && armies.engagement_sides[armyIndex] == side && armies.owners[armyIndex] == owner)
			total += army_total_manpower(armies.compositions[armyIndex]);
	}
	return total;
}

void world_engagement_update_commanders(match_world_state& world, entity_guid engagement)
{
	if (!world_entity_is_valid(world, engagement) || engagement.get_type() != ENTITY_TYPE::ENGAGEMENT) return;

	const world_entity_armies& armies = world.entities->armies;
	world_entity_engagements::state& state = world.entities->engagements.states[engagement.get_index()];

	// A siege's defender commander is its settlement's owner, set at spawn and never recomputed.
	for (ui8 side = world_entity_engagements::SIDE_ATTACKER; side <= world_entity_engagements::SIDE_DEFENDER; side++)
	{
		if (state.kind == world_entity_engagements::KIND_SIEGE && side == world_entity_engagements::SIDE_DEFENDER) continue;

		// @TODO(Marc): P4: the leader army's owner of a campaign overrides this (campaign leader commands).
		// The owner with the most men commands; the incumbent keeps command on a tie (and when nobody is left).
		world_entity_engagements::side_state& sideState = state.side(side);
		match_player_id best = sideState.commander;
		ui32 bestMen = engagement_owner_men(world, engagement, side, best);
		for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			if (!armies.guids[armyIndex].is_valid() || !(armies.engagements[armyIndex] == engagement) || armies.engagement_sides[armyIndex] != side) continue;

			ui32 men = engagement_owner_men(world, engagement, side, armies.owners[armyIndex]);
			if (men > bestMen)
			{
				best = armies.owners[armyIndex];
				bestMen = men;
			}
		}
		sideState.commander = best;
	}
}

void world_engagement_end(match_world_state& world, entity_guid engagement)
{
	world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (armies.guids[armyIndex].is_valid() && armies.engagements[armyIndex] == engagement) armies.engagements[armyIndex] = INVALID_ENTITY_GUID;
	}
	world_destroy_engagement(world, engagement);
}

void world_siege_check_attackers(match_world_state& world, entity_guid engagement)
{
	if (!world_entity_is_valid(world, engagement) || engagement.get_type() != ENTITY_TYPE::ENGAGEMENT) return;
	if (world.entities->engagements.states[engagement.get_index()].kind != world_entity_engagements::KIND_SIEGE) return;

	const world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (armies.guids[armyIndex].is_valid() && armies.engagements[armyIndex] == engagement && armies.engagement_sides[armyIndex] == world_entity_engagements::SIDE_ATTACKER) return;
	}
	world_engagement_end(world, engagement);
}

void world_engagement_army_left(match_world_state& world, entity_guid engagement)
{
	world_siege_check_attackers(world, engagement);
	world_engagement_update_commanders(world, engagement); // Does nothing if the engagement just ended.
}

PLAYER_DIPLOMATIC_STATUS engagement_diplomatic_status(const match_world_state& world, entity_guid engagement, match_player_id player)
{
	const world_entity_engagements::state& state = world.entities->engagements.states[engagement.get_index()];

	// Rating of each side for the player: 2 friendly, 1 neutral, 0 hostile.
	ui8 rating[2];
	for (ui8 side = world_entity_engagements::SIDE_ATTACKER; side <= world_entity_engagements::SIDE_DEFENDER; side++)
	{
		PLAYER_DIPLOMATIC_STATUS commanderStatus = world_player_get_diplomatic_status(world, player, state.side(side).commander);
		rating[side] = commanderStatus == PLAYER_DIPLOMATIC_STATUS_FRIENDLY ? 2 : (commanderStatus == PLAYER_DIPLOMATIC_STATUS_HOSTILE ? 0 : 1);
	}

	if (rating[world_entity_engagements::SIDE_ATTACKER] == rating[world_entity_engagements::SIDE_DEFENDER]) return PLAYER_DIPLOMATIC_STATUS_NEUTRAL;
	return rating[world_entity_engagements::SIDE_ATTACKER] > rating[world_entity_engagements::SIDE_DEFENDER] ? PLAYER_DIPLOMATIC_STATUS_FRIENDLY : PLAYER_DIPLOMATIC_STATUS_HOSTILE;
}

ui32 engagement_join_radius(const match_world_state& world, entity_guid engagement)
{
	const world_entity_engagements::state& state = world.entities->engagements.states[engagement.get_index()];

	ui64 troops = 0;
	for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++) troops += (ui64)state.attacker.men[unitType] + state.defender.men[unitType];

	// A siege's defender side holds the garrison (inside the settlement) unless it is an outside clash, or the settlement is the attackers' already: not counted.
	if (state.kind == world_entity_engagements::KIND_SIEGE && state.phase != world_entity_engagements::PHASE_OUTSIDE && world_entity_is_valid(world, state.main_target)
		&& world.entities->settlements.owners[state.main_target.get_index()] != state.attacker.commander)
	{
		troops -= ia_min((ui64)army_total_manpower(world.entities->settlements.garrison[state.main_target.get_index()]), troops);
	}

	ui64 threshold = ia_max(GAME_PARAMETERS.engagement_join_troops_threshold, (ui32)1);
	ui32 powersAbove = troops >= 2 * threshold ? ia_log2_q8(troops / threshold) / 256 : 0; // Integer part of log2.
	return GAME_PARAMETERS.engagement_join_base_tiles + powersAbove;
}

bool world_entity_query_engagement(const match_world_state& world, entity_guid entity, match_player_id viewer, world_entity_engagements::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::ENGAGEMENT || !world_entity_is_valid(world, entity)) return false;

	const world_entity_engagements& engagements = world.entities->engagements;
	ui16 index = entity.get_index();
	out_state.copy_from(engagements, index);
	out_state.join_radius = (ui8)ia_min(engagement_join_radius(world, entity), (ui32)255);
	out_state.viewer_status = (ui8)engagement_diplomatic_status(world, entity, viewer);
	return true;
}

bool world_alloc_engagements(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_engagements& engagements = world.entities->engagements;

	engagements.max_count = count;

	engagements.guids = memory.alloc<entity_guid>(count);
	if (engagements.guids == nullptr) return false;

	engagements.states = memory.alloc<world_entity_engagements::state>(count);
	if (engagements.states == nullptr) return false;

	return true;
}
