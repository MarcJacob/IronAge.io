#include "world_sim_entity_engagement.h"
#include "game_common/match/world/troop_types.h"
#include "world_sim_entity_army.h"
#include "world_sim_entity_settlement.h"
#include "core/math/random.h"
#include "../world_sim.h"
#include "../world_movement.h"

entity_guid world_spawn_engagement(match_world_state& world, const world_entity_engagement& start_state)
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
	newEngagement.phase_tick = world.age;
	engagements.guids[freeIndex] = newID;
	engagements.active_count++;

	// A siege is recorded on the besieged settlement (cleared in world_destroy_engagement).
	if (start_state.kind == ENGAGEMENT_KIND::SIEGE)
	{
		ASSERT(start_state.main_target.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, start_state.main_target));
		world.entities->settlements.siege[start_state.main_target.get_index()] = newID;
	}

	return newID;
}

void world_destroy_engagement(match_world_state& world, entity_guid engagement)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	world_entity_engagements& engagements = world.entities->engagements;

	ui16 index = engagement.get_index();

	// Clear the besieged settlement's siege (it may be gone already, or under another siege by now).
	entity_guid endedTarget = engagements.main_target[index];
	if (engagements.kind[index] == ENGAGEMENT_KIND::SIEGE && endedTarget.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, endedTarget))
	{
		entity_guid& siege = world.entities->settlements.siege[endedTarget.get_index()];
		if (siege == engagement) siege = INVALID_ENTITY_GUID;
	}

	world_entity_engagement_view destroyedEngagement { engagements, index };
	destroyedEngagement.copy_from(world_entity_engagement {});
	engagements.guids[index] = INVALID_ENTITY_GUID;
	engagements.active_count--;
}

void world_army_join_engagement(match_world_state& world, entity_guid army, entity_guid engagement, ENGAGEMENT_SIDE side)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();

	for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
	{
		world.entities->engagements.side_troops(side, engagement.get_index()).count[unitType] += armies.composition[armyIndex].count[unitType];
	}

	armies.engagement[armyIndex] = engagement;
	armies.engagement_side[armyIndex] = side;
	// The army's order is kept and goes on after the engagement (it is not worked on while pinned).
	armies.order[armyIndex].engaged = INVALID_ENTITY_GUID;
	world_movement_stop(armies.movement[armyIndex], armies.location[armyIndex]);
	world_engagement_update_commanders(world, engagement);
}

bool world_army_pick_engagement_side(const match_world_state& world, entity_guid army, entity_guid engagement, ENGAGEMENT_SIDE& out_side)
{
	const world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();

	// An army that cannot inflict damage joins nothing.
	if (troops_base_damage(armies.composition[armyIndex]) == 0) return false;

	PLAYER_DIPLOMATIC_STATUS status = engagement_diplomatic_status(world, engagement, armies.owner[armyIndex]);
	if (status == PLAYER_DIPLOMATIC_STATUS_NEUTRAL) return false;

	const world_entity_engagements& engagements = world.entities->engagements;
	ui16 engagementIndex = engagement.get_index();
	// Friendly = join the attackers, hostile = join the defenders.
	out_side = status == PLAYER_DIPLOMATIC_STATUS_FRIENDLY ? ENGAGEMENT_SIDE::ATTACKER : ENGAGEMENT_SIDE::DEFENDER;

	// An army standing on the besieged settlement does not take the defender side of its siege.
	if (engagements.kind[engagementIndex] == ENGAGEMENT_KIND::SIEGE && out_side == ENGAGEMENT_SIDE::DEFENDER && world_entity_is_valid(world, engagements.main_target[engagementIndex])
		&& armies.location[armyIndex] == world.entities->settlements.location[engagements.main_target[engagementIndex].get_index()]) return false;

	return true;
}

bool world_army_join_engagement(match_world_state& world, entity_guid army, entity_guid engagement)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	ASSERT(!armies.engagement[armyIndex].is_valid()); // Callers make sure the army is free. Empty armies should not be passed either.
	ASSERT(army_total_manpower(armies.composition[armyIndex]) > 0);

	ENGAGEMENT_SIDE side;
	if (!world_army_pick_engagement_side(world, army, engagement, side)) return false;

	world_entity_engagements& engagements = world.entities->engagements;
	ui16 engagementIndex = engagement.get_index();
	world_army_join_engagement(world, army, engagement, side);
	if (engagements.kind[engagementIndex] == ENGAGEMENT_KIND::SIEGE && side == ENGAGEMENT_SIDE::DEFENDER && engagements.phase[engagementIndex] == ENGAGEMENT_PHASE::SIEGE_IDLE)
	{
		engagements.phase[engagementIndex] = ENGAGEMENT_PHASE::SIEGE_OUTSIDE_BATTLE;
		engagements.phase_tick[engagementIndex] = world.age;
		engagements.fighting_active[engagementIndex] = true; // Defending armies clash with the besiegers.
	}
	return true;
}

bool world_engagement_can_be_left(const match_world_state& world, entity_guid engagement, world_location location, ENGAGEMENT_SIDE side)
{
	const world_entity_engagements& engagements = world.entities->engagements;
	ui16 engagementIndex = engagement.get_index();

	// Far enough away.
	float leaveRadius = (float)(engagement_join_radius(world, engagement) * GAME_PARAMETERS.engagement_leave_radius_multiplier);
	if (vec2_dist_squared(location, engagements.location[engagementIndex]) > leaveRadius * leaveRadius) return true;

	// Otherwise only the attackers of an idle siege: nothing fights there, they just walk out.
	return engagements.kind[engagementIndex] == ENGAGEMENT_KIND::SIEGE && !engagements.fighting_active[engagementIndex] && side == ENGAGEMENT_SIDE::ATTACKER;
}

bool world_army_can_leave_current_engagement(const match_world_state& world, entity_guid army)
{
	const world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();
	if (!armies.engagement[armyIndex].is_valid()) return false;

	return world_engagement_can_be_left(world, armies.engagement[armyIndex], armies.location[armyIndex], armies.engagement_side[armyIndex]);
}

void world_army_leave_current_engagement(match_world_state& world, entity_guid army)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();
	entity_guid engagement = armies.engagement[armyIndex];
	if (!engagement.is_valid()) return;

	// Its men leave their side (a siege recounts its sides every round anyway).
	for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
		world_engagement_remove_troops(world, engagement, armies.engagement_side[armyIndex], (TROOP_TYPE)unitType, armies.composition[armyIndex].count[unitType]);

	armies.engagement[armyIndex] = INVALID_ENTITY_GUID;
	world_engagement_army_left(world, engagement); // May end a siege left without attackers.
}

entity_guid world_engagement_start_siege(match_world_state& world, entity_guid attacker, entity_guid settlement)
{
	world_entity_settlements& settlements = world.entities->settlements;
	world_entity_armies& armies = world.entities->armies;
	ui16 attackerIndex = attacker.get_index();
	ui16 settlementIndex = settlement.get_index();

	// The settlement's garrison is the defending side, it is recounted into the engagement at every round (see world_system_engagements).
	// A siege starts idle (besieging): no damage until the attacker commander begins the assault.
	world_entity_engagement start = {};
	start.defender_troops = settlements.garrison[settlementIndex];
	start.location = (armies.location[attackerIndex] + settlements.location[settlementIndex]) / 2; // Start engagement at midpoint between belligerents.
	start.kind = ENGAGEMENT_KIND::SIEGE;
	start.phase = ENGAGEMENT_PHASE::SIEGE_IDLE;
	start.fighting_active = false;
	start.main_target = settlement;
	start.attacker_commander = armies.owner[attackerIndex];
	start.defender_commander = settlements.owner[settlementIndex];
	entity_guid engagement = world_spawn_engagement(world, start);
	world_army_join_engagement(world, attacker, engagement, ENGAGEMENT_SIDE::ATTACKER);
	return engagement;
}

entity_guid world_engagement_start_field_battle(match_world_state& world, entity_guid attacker, entity_guid target)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 attackerIndex = attacker.get_index();
	ui16 targetIndex = target.get_index();

	world_entity_engagement start = {};
	start.location = (armies.location[attackerIndex] + armies.location[targetIndex]) / 2;
	start.kind = ENGAGEMENT_KIND::FIELD_BATTLE;
	start.phase = ENGAGEMENT_PHASE::ACTIVE;
	start.fighting_active = true;
	start.main_target = target;
	start.attacker_commander = armies.owner[attackerIndex];
	start.defender_commander = armies.owner[targetIndex];
	entity_guid engagement = world_spawn_engagement(world, start);
	world_army_join_engagement(world, attacker, engagement, ENGAGEMENT_SIDE::ATTACKER);
	world_army_join_engagement(world, target, engagement, ENGAGEMENT_SIDE::DEFENDER);
	return engagement;
}

void world_engagement_siege_begin_assault(match_world_state& world, entity_guid engagement)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	world_entity_engagements& engagements = world.entities->engagements;
	ui16 index = engagement.get_index();
	ASSERT(engagements.kind[index] == ENGAGEMENT_KIND::SIEGE && engagements.phase[index] == ENGAGEMENT_PHASE::SIEGE_IDLE);

	engagements.phase[index] = ENGAGEMENT_PHASE::SIEGE_ASSAULT;
	engagements.phase_tick[index] = world.age;
	engagements.fighting_active[index] = true;
}

void world_engagement_siege_sally_out(match_world_state& world, entity_guid engagement)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	world_entity_engagements& engagements = world.entities->engagements;
	ui16 index = engagement.get_index();
	ASSERT(engagements.kind[index] == ENGAGEMENT_KIND::SIEGE && (engagements.phase[index] == ENGAGEMENT_PHASE::SIEGE_IDLE || engagements.phase[index] == ENGAGEMENT_PHASE::SIEGE_OUTSIDE_BATTLE));

	// The garrison joins the defender side at the next round (the siege recounts its sides then).
	engagements.phase[index] = ENGAGEMENT_PHASE::SIEGE_SALLY;
	engagements.phase_tick[index] = world.age;
	engagements.fighting_active[index] = true;
}

void world_engagement_siege_lift(match_world_state& world, entity_guid engagement)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	ASSERT(world.entities->engagements.kind[engagement.get_index()] == ENGAGEMENT_KIND::SIEGE);
	ASSERT(world.entities->engagements.phase[engagement.get_index()] == ENGAGEMENT_PHASE::SIEGE_IDLE);

	// Every army freed stands where it is (its order would otherwise take it straight back into a new siege).
	world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !(armies.engagement[armyIndex] == engagement)) continue;

		army_order_station(armies, (ui16)armyIndex, armies.location[armyIndex]);
		world_movement_stop(armies.movement[armyIndex], armies.location[armyIndex]);
	}

	world_engagement_end(world, engagement);
}

ui16 world_engagement_remove_troops(match_world_state& world, entity_guid engagement, ENGAGEMENT_SIDE side, TROOP_TYPE troop_type, ui16 count)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	ui16& troops = world.entities->engagements.side_troops(side, engagement.get_index()).access_troop_count(troop_type);
	ui16 removed = ia_min(count, troops);
	troops -= removed;
	return removed;
}

// Total men of the armies of one owner on one side of a engagement.
static ui32 engagement_owner_men(const match_world_state& world, entity_guid engagement, ENGAGEMENT_SIDE side, match_player_id owner)
{
	const world_entity_armies& armies = world.entities->armies;
	ui32 total = 0;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (armies.guids[armyIndex].is_valid() && armies.engagement[armyIndex] == engagement && armies.engagement_side[armyIndex] == side && armies.owner[armyIndex] == owner)
			total += army_total_manpower(armies.composition[armyIndex]);
	}
	return total;
}

void world_engagement_update_commanders(match_world_state& world, entity_guid engagement)
{
	if (!world_entity_is_valid(world, engagement) || engagement.get_type() != ENTITY_TYPE::ENGAGEMENT) return;

	const world_entity_armies& armies = world.entities->armies;
	world_entity_engagements& engagements = world.entities->engagements;
	ui16 engagementIndex = engagement.get_index();

	// A siege's defender commander is its settlement's owner, set at spawn and never recomputed.
	for (ui8 sideIndex = 0; sideIndex < (ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT; sideIndex++)
	{
		ENGAGEMENT_SIDE side = (ENGAGEMENT_SIDE)sideIndex;
		if (engagements.kind[engagementIndex] == ENGAGEMENT_KIND::SIEGE && side == ENGAGEMENT_SIDE::DEFENDER) continue;

		// @TODO(Marc): P4: the leader army's owner of a campaign overrides this (campaign leader commands).
		// The owner with the most men commands; the incumbent keeps command on a tie (and when nobody is left).
		match_player_id best = engagements.side_commander(side, engagementIndex);
		ui32 bestMen = engagement_owner_men(world, engagement, side, best);
		for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			if (!armies.guids[armyIndex].is_valid() || !(armies.engagement[armyIndex] == engagement) || armies.engagement_side[armyIndex] != side) continue;

			ui32 men = engagement_owner_men(world, engagement, side, armies.owner[armyIndex]);
			if (men > bestMen)
			{
				best = armies.owner[armyIndex];
				bestMen = men;
			}
		}
		engagements.side_commander(side, engagementIndex) = best;
	}
}

// LOOT

// Adds to a loot pool, saturating.
static void loot_pool_add(ui32& pool, ui64 amount)
{
	pool = (ui32)ia_min((ui64)pool + amount, (ui64)0xFFFFFFFF);
}

void world_engagement_loot_add(match_world_state& world, entity_guid engagement, ui64 gold, ui64 supplies)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	loot_pool_add(world.entities->engagements.loot_gold[engagement.get_index()], gold);
	loot_pool_add(world.entities->engagements.loot_supplies[engagement.get_index()], supplies);
}

// Kills part of a settlement's population (conquest) and returns the gold it yields (1 each). Soldiers at home count in the urban population the percentage applies to,
// but only unemployed and workers can be killed here.
static ui32 settlement_conquest_loot(match_world_state& world, ui16 settlement_index)
{
	world_entity_settlements& settlements = world.entities->settlements;
	const game_parameters& params = GAME_PARAMETERS;

	ui32 peasantsKilled = settlement_remove_peasants(world, settlement_index, (ui32)((ui64)settlements.pop_peasants[settlement_index] * ia_min(params.conquest_loot_peasants_percent, (ui32)100) / 100));

	ui64 urbanTarget = (ui64)settlements.urban_pop(settlement_index) * ia_min(params.conquest_loot_urban_percent, (ui32)100) / 100;
	ui32 civilians = settlements.pop_unemployed[settlement_index] + settlements.pop_workers[settlement_index];
	ui32 urbanKilled = settlement_kill_population(world, settlement_index, (ui32)ia_min(urbanTarget, (ui64)civilians));

	return peasantsKilled + urbanKilled;
}

// Shares a pool (gold or supplies) among the qualifying armies: those of the side of the engagement, or only_army if valid. By manpower, each capped by the room
// left in its own gold / supplies pool. What the capped armies could not take is shared again among those with room left, until the pool is empty or nobody has room.
static void loot_share(match_world_state& world, ui32& pool, bool is_gold, entity_guid engagement, ENGAGEMENT_SIDE side, entity_guid only_army)
{
	world_entity_armies& armies = world.entities->armies;

	while (pool > 0)
	{
		// Armies with room left.
		ui64 totalMen = 0;
		for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			if (!armies.guids[armyIndex].is_valid()) continue;
			if (only_army.is_valid() ? !(armies.guids[armyIndex] == only_army) : (!(armies.engagement[armyIndex] == engagement) || armies.engagement_side[armyIndex] != side)) continue;

			ui32 stock = is_gold ? armies.gold[armyIndex] : armies.supplies[armyIndex];
			if (army_supply_capacity(world, (ui16)armyIndex) > stock) totalMen += army_total_manpower(armies.composition[armyIndex]);
		}
		if (totalMen == 0) break;

		ui32 poolBefore = pool;
		for (ui32 armyIndex = 0; armyIndex < armies.max_count && pool > 0; armyIndex++)
		{
			if (!armies.guids[armyIndex].is_valid()) continue;
			if (only_army.is_valid() ? !(armies.guids[armyIndex] == only_army) : (!(armies.engagement[armyIndex] == engagement) || armies.engagement_side[armyIndex] != side)) continue;

			ui32& stock = is_gold ? armies.gold[armyIndex] : armies.supplies[armyIndex];
			ui32 capacity = army_supply_capacity(world, (ui16)armyIndex);
			if (capacity <= stock) continue;

			// Rounded up so a pass always gives something away.
			ui64 men = army_total_manpower(armies.composition[armyIndex]);
			ui64 share = ((ui64)poolBefore * men + totalMen - 1) / totalMen;
			share = ia_min(ia_min(share, (ui64)(capacity - stock)), (ui64)pool);
			stock += (ui32)share;
			pool -= (ui32)share;
		}
		if (pool == poolBefore) break;
	}
}

// What is left of the pools is looted by the local population: gold becomes trade volume and supplies become goods of the settlement. Nothing if there is none.
static void loot_leftover_to_settlement(match_world_state& world, ui16 settlement_index, ui32 gold, ui32 supplies)
{
	world_entity_settlements& settlements = world.entities->settlements;
	if (settlement_index >= settlements.max_count || !settlements.guids[settlement_index].is_valid()) return;

	settlement_trading& trade = settlements.trade[settlement_index];
	trade.trade_volume = (ui32)ia_min((ui64)trade.trade_volume + gold, (ui64)0xFFFFFFFF);
	settlement_market_add(settlements, settlement_index, supplies);
}

void world_army_conquest_loot(match_world_state& world, entity_guid army, ui16 settlement_index)
{
	ui32 gold = settlement_conquest_loot(world, settlement_index);
	ui32 supplies = 0;
	loot_share(world, gold, true, INVALID_ENTITY_GUID, ENGAGEMENT_SIDE::ATTACKER, army);
	loot_leftover_to_settlement(world, settlement_index, gold, supplies);
}

void world_engagement_on_finish(match_world_state& world, entity_guid engagement, bool has_winner, ENGAGEMENT_SIDE winner)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	world_entity_engagements& engagements = world.entities->engagements;
	ui16 index = engagement.get_index();
	entity_guid mainTarget = engagements.main_target[index];
	bool isSiege = engagements.kind[index] == ENGAGEMENT_KIND::SIEGE;
	bool targetValid = world_entity_is_valid(world, mainTarget) && mainTarget.get_type() == ENTITY_TYPE::SETTLEMENT;

	// Attackers winning a siege conquer the settlement: its people pay.
	if (has_winner && isSiege && winner == ENGAGEMENT_SIDE::ATTACKER && targetValid)
		loot_pool_add(engagements.loot_gold[index], settlement_conquest_loot(world, mainTarget.get_index()));

	if (has_winner)
	{
		loot_share(world, engagements.loot_gold[index], true, engagement, winner, INVALID_ENTITY_GUID);
		loot_share(world, engagements.loot_supplies[index], false, engagement, winner, INVALID_ENTITY_GUID);
	}

	// Leftover: the besieged settlement, or the settlement whose territory the engagement is in.
	ui16 settlementIndex = 0xFFFF;
	if (isSiege && targetValid)
	{
		settlementIndex = mainTarget.get_index();
	}
	else
	{
		ui16 regionIndex, tileIndex;
		world_get_tile(*world.terrain, engagements.location[index], regionIndex, tileIndex);
		settlementIndex = world.terrain->regions[regionIndex].owners[tileIndex];
		if (settlementIndex == world_terrain::UNOWNED_TILE) settlementIndex = 0xFFFF;
	}
	loot_leftover_to_settlement(world, settlementIndex, engagements.loot_gold[index], engagements.loot_supplies[index]);

	engagements.loot_gold[index] = 0;
	engagements.loot_supplies[index] = 0;
}

void world_engagement_end(match_world_state& world, entity_guid engagement)
{
	// Reaching here without a verdict (siege lifted, attackers gone): the defender side wins a siege, a field battle has no winner.
	world_engagement_on_finish(world, engagement, world.entities->engagements.kind[engagement.get_index()] == ENGAGEMENT_KIND::SIEGE, ENGAGEMENT_SIDE::DEFENDER);

	world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (armies.guids[armyIndex].is_valid() && armies.engagement[armyIndex] == engagement) armies.engagement[armyIndex] = INVALID_ENTITY_GUID;
	}
	world_destroy_engagement(world, engagement);
}

void world_siege_check_attackers(match_world_state& world, entity_guid engagement)
{
	if (!world_entity_is_valid(world, engagement) || engagement.get_type() != ENTITY_TYPE::ENGAGEMENT) return;
	if (world.entities->engagements.kind[engagement.get_index()] != ENGAGEMENT_KIND::SIEGE) return;

	const world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (armies.guids[armyIndex].is_valid() && armies.engagement[armyIndex] == engagement && armies.engagement_side[armyIndex] == ENGAGEMENT_SIDE::ATTACKER) return;
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
	const world_entity_engagements& engagements = world.entities->engagements;

	// Rating of each side for the player: 2 friendly, 1 neutral, 0 hostile.
	ui8 rating[(ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT];
	for (ui8 sideIndex = 0; sideIndex < (ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT; sideIndex++)
	{
		PLAYER_DIPLOMATIC_STATUS commanderStatus = world_player_get_diplomatic_status(world, player, engagements.side_commander((ENGAGEMENT_SIDE)sideIndex, engagement.get_index()));
		rating[sideIndex] = commanderStatus == PLAYER_DIPLOMATIC_STATUS_FRIENDLY ? 2 : (commanderStatus == PLAYER_DIPLOMATIC_STATUS_HOSTILE ? 0 : 1);
	}

	ui8 attackerRating = rating[(ui8)ENGAGEMENT_SIDE::ATTACKER];
	ui8 defenderRating = rating[(ui8)ENGAGEMENT_SIDE::DEFENDER];
	if (attackerRating == defenderRating) return PLAYER_DIPLOMATIC_STATUS_NEUTRAL;
	return attackerRating > defenderRating ? PLAYER_DIPLOMATIC_STATUS_FRIENDLY : PLAYER_DIPLOMATIC_STATUS_HOSTILE;
}

ui32 engagement_join_radius(const match_world_state& world, entity_guid engagement)
{
	const world_entity_engagements& engagements = world.entities->engagements;
	ui16 index = engagement.get_index();
	entity_guid mainTarget = engagements.main_target[index];

	ui64 troops = army_total_manpower(engagements.attacker_troops[index]) + army_total_manpower(engagements.defender_troops[index]);

	// A siege's defender side holds the garrison (inside the settlement) unless it is an outside clash, or the settlement is the attackers' already: not counted.
	if (engagements.kind[index] == ENGAGEMENT_KIND::SIEGE && engagements.phase[index] != ENGAGEMENT_PHASE::SIEGE_OUTSIDE_BATTLE && world_entity_is_valid(world, mainTarget)
		&& world.entities->settlements.owner[mainTarget.get_index()] != engagements.attacker_commander[index])
	{
		troops -= ia_min((ui64)army_total_manpower(world.entities->settlements.garrison[mainTarget.get_index()]), troops);
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

	engagements.location = memory.alloc<world_location>(count);
	if (engagements.location == nullptr) return false;

	engagements.kind = memory.alloc<ENGAGEMENT_KIND>(count);
	if (engagements.kind == nullptr) return false;

	engagements.phase = memory.alloc<ENGAGEMENT_PHASE>(count);
	if (engagements.phase == nullptr) return false;

	engagements.main_target = memory.alloc<entity_guid>(count);
	if (engagements.main_target == nullptr) return false;

	engagements.phase_tick = memory.alloc<ui32>(count);
	if (engagements.phase_tick == nullptr) return false;

	engagements.fighting_active = memory.alloc<bool>(count);
	if (engagements.fighting_active == nullptr) return false;

	engagements.duration = memory.alloc<ui32>(count);
	if (engagements.duration == nullptr) return false;

	engagements.loot_gold = memory.alloc<ui32>(count);
	if (engagements.loot_gold == nullptr) return false;

	engagements.loot_supplies = memory.alloc<ui32>(count);
	if (engagements.loot_supplies == nullptr) return false;

	engagements.attacker_troops = memory.alloc<army_troop_count>(count);
	if (engagements.attacker_troops == nullptr) return false;

	engagements.attacker_damage = memory.alloc<ui32>(count);
	if (engagements.attacker_damage == nullptr) return false;

	engagements.attacker_commander = memory.alloc<match_player_id>(count);
	if (engagements.attacker_commander == nullptr) return false;

	engagements.defender_troops = memory.alloc<army_troop_count>(count);
	if (engagements.defender_troops == nullptr) return false;

	engagements.defender_damage = memory.alloc<ui32>(count);
	if (engagements.defender_damage == nullptr) return false;

	engagements.defender_commander = memory.alloc<match_player_id>(count);
	if (engagements.defender_commander == nullptr) return false;

	return true;
}

// ENGAGEMENT SYSTEMS: joining engagements and resolving engagement rounds. Siege outcomes are resolved here too. Declared in systems_war.h.

// Every tick: armies that are free (not in an engagement) and have men, within the join radius of an engagement join it, if it is joinable for them ("aggro").
// Not if they could leave it right away (e.g. attackers of an idle siege): their order logic decides to join those, so orders and joining never fight each other.
void world_system_engagement_auto_join(match_world_state& world)
{
	world_entity_engagements& engagements = world.entities->engagements;
	world_entity_armies& armies = world.entities->armies;

	for (ui32 engagementIndex = 0; engagementIndex < engagements.max_count; engagementIndex++)
	{
		entity_guid engagement = engagements.guids[engagementIndex];
		if (!engagement.is_valid()) continue;

		float radius = (float)engagement_join_radius(world, engagement);
		for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			if (!armies.guids[armyIndex].is_valid() || armies.engagement[armyIndex].is_valid()) continue;
			if (army_total_manpower(armies.composition[armyIndex]) == 0) continue;
			if (vec2_dist_squared(armies.location[armyIndex], engagements.location[engagementIndex]) > radius * radius) continue;

			// An army that could leave right away (attacker of an idle siege...) is not aggroed: its order logic joins it if it wants to.
			ENGAGEMENT_SIDE side;
			if (!world_army_pick_engagement_side(world, armies.guids[armyIndex], engagement, side)) continue;
			if (world_engagement_can_be_left(world, engagement, armies.location[armyIndex], side)) continue;

			world_army_join_engagement(world, armies.guids[armyIndex], engagement);
		}
	}
}

ui32 troops_base_damage(const army_troop_count& troops)
{
	ui32 base = 0;
	for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
		base += (ui32)troops.count[unitType] * GAME_PARAMETERS.troop_type_stats[unitType].damage;
	return base;
}

// Damage points a side deals this round, before the defences of the other side: 
// its men's total damage x a random percent in [min, max] x engagement_damage_scale_percent (rounded up).
// One RNG roll.
static ui32 engagement_roll_side_damage(match_world_state& world, const army_troop_count& troops)
{
	ui32 minPercent = GAME_PARAMETERS.engagement_damage_min_percent;
	ui32 maxPercent = ia_max(GAME_PARAMETERS.engagement_damage_max_percent, minPercent);
	ui32 percent = world.rand_gen.next_range<ui32>(minPercent, maxPercent + 1);
	if (percent > maxPercent) percent = maxPercent; // The range function may land on its max.

	ui32 base = troops_base_damage(troops);
    
    // Apply random effective percent + global scale percent, divided by 10000 (with 9999 added to ensure that any amount of damage inflicted leads to 1 effective damage point no matter what).
	return (base * percent * GAME_PARAMETERS.engagement_damage_scale_percent + 9999) / 10000;
}

// Men killed in a group (an army or the garrison) of a side that deals with the damage of the other side. The group takes its men's share of the damage
// (sideDamage x men of the type / sideMen, rounded up), so damage is spread over armies and unit types in proportion to their men.
// Per unit type present, in order: kills = (aimed damage / rand(1, defense)) / rand(1, armor), at most the men of that type. Two RNG rolls per unit type.
static void engagement_group_kills(match_world_state& world, ui64 sideDamage, ui64 sideMen, const army_troop_count& troops, army_troop_count& out_kills)
{
	for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
	{
		out_kills.count[unitType] = 0;
		if (troops.count[unitType] == 0) continue;

		const game_parameters::troop_type_stat_def& stats = GAME_PARAMETERS.troop_type_stats[unitType];
		ui64 aimed = (sideDamage * troops.count[unitType] + sideMen - 1) / sideMen;

		ui32 defense = ia_max((ui32)stats.defense, (ui32)1);
		ui32 armor = ia_max((ui32)stats.armor, (ui32)1);
		// The range function may land on its max (see engagement_roll_side_damage).
		ui32 defenseRoll = ia_min(world.rand_gen.next_range<ui32>(1, defense + 1), defense);
		ui32 armorRoll = ia_min(world.rand_gen.next_range<ui32>(1, armor + 1), armor);
		ui64 kills = aimed / defenseRoll / armorRoll;
		out_kills.count[unitType] = (ui16)ia_min(kills, (ui64)troops.count[unitType]);
	}
}

void world_system_engagements(match_world_state& world)
{
	if (world.age % ia_max(GAME_PARAMETERS.engagement_tick_interval, (ui32)1) != 0) return;

	world_entity_engagements& engagements = world.entities->engagements;
	world_entity_armies& armies = world.entities->armies;

	for (ui32 engagementIndex = 0; engagementIndex < engagements.max_count; engagementIndex++)
	{
		if (!engagements.guids[engagementIndex].is_valid()) continue;

		const entity_guid engagementGuid = engagements.guids[engagementIndex];

		// Siege: the besieged settlement's garrison is part of the defender side (unless the settlement is already the attackers'). The sides are recounted from their
		// armies and the garrison, which can change between rounds (training, desertion).
		ui16 garrisonIndex = 0xFFFF;
		ui64 garrisonMen = 0;

		bool isSiege = engagements.kind[engagementIndex] == ENGAGEMENT_KIND::SIEGE 
            && world_entity_is_valid(world, engagements.main_target[engagementIndex]);
		if (isSiege)
		{
			engagements.attacker_troops[engagementIndex] = {};
			engagements.defender_troops[engagementIndex] = {};
			for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
			{
				if (!armies.guids[armyIndex].is_valid() || !(armies.engagement[armyIndex] == engagementGuid)) continue;

				army_troop_count& sideTroops = engagements.side_troops(armies.engagement_side[armyIndex], engagementIndex);
				for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++) 
                {
                    sideTroops.count[unitType] += armies.composition[armyIndex].count[unitType];
                }
			}

			ui16 settlementIndex = engagements.main_target[engagementIndex].get_index();

			// Not during an outside clash: the garrison (and any army inside) stays out of it.
			if (world.entities->settlements.owner[settlementIndex] != engagements.attacker_commander[engagementIndex] && engagements.phase[engagementIndex] != ENGAGEMENT_PHASE::SIEGE_OUTSIDE_BATTLE)
			{
				garrisonIndex = settlementIndex;
				for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
				{
					ui16 count = world.entities->settlements.garrison[garrisonIndex].count[unitType];
					engagements.defender_troops[engagementIndex].count[unitType] += count;
					garrisonMen += count;
				}
			}
		}

		// Men per side at the start of the tick. Armies take damage in proportion to their share of it.
		ui64 sideMen[(ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT] = {}; // Indexed by ENGAGEMENT_SIDE.
		for (ui8 sideIndex = 0; sideIndex < (ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT; sideIndex++) 
        {
            sideMen[sideIndex] = army_total_manpower(engagements.side_troops((ENGAGEMENT_SIDE)sideIndex, engagementIndex));
        }

		ui64 attackerMen = sideMen[(ui8)ENGAGEMENT_SIDE::ATTACKER];
		ui64 defenderMen = sideMen[(ui8)ENGAGEMENT_SIDE::DEFENDER];

		// Zero-damage rule (fighting only, not an idle siege): a side that cannot deal damage (only wagons...) loses even with men left. It counts as wiped below:
		// attacker wiped = siege lifted / attackers gone, defender wiped = conquest (outside clash: back to besieging). Both harmless = the engagement ends with no winner
		// (the attacker counts as wiped, so no conquest). Armies are only released, never destroyed by this rule.
		if (engagements.fighting_active[engagementIndex])
		{
			if (troops_base_damage(engagements.attacker_troops[engagementIndex]) == 0) attackerMen = 0;
			if (troops_base_damage(engagements.defender_troops[engagementIndex]) == 0) defenderMen = 0;
		}

		// Commanders follow the men (before any casualty of this round).
		world_engagement_update_commanders(world, engagementGuid);

		// A wiped side means the engagement is over: its armies are already dead, the others are released below.
		// Sieges: defender side wiped = conquest by the attacker commander (below); attacker side wiped = siege lifted.
		// An outside clash is the exception: its defender side is only the relief armies, and losing them sends the siege back to besieging (below).

		bool outside = isSiege && engagements.phase[engagementIndex] == ENGAGEMENT_PHASE::SIEGE_OUTSIDE_BATTLE;

		bool over = attackerMen == 0 || (isSiege && (defenderMen == 0 && !outside) || !isSiege && (defenderMen == 0));

		// Relief armies wiped (attacker side won the outside clash): the siege goes on, idle again.
		if (!over && outside && defenderMen == 0)
		{
			engagements.phase[engagementIndex] = ENGAGEMENT_PHASE::SIEGE_IDLE;
			engagements.phase_tick[engagementIndex] = world.age;
			engagements.duration[engagementIndex] = 0;
			engagements.fighting_active[engagementIndex] = false; // Fighting stops: back to the idle siege.

			// Relief armies still in it can only be harmless ones (wagons...): the idle siege would pin them forever (only attackers can leave an idle siege). They are released.
			for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
			{
				if (!armies.guids[armyIndex].is_valid() || !(armies.engagement[armyIndex] == engagementGuid) || armies.engagement_side[armyIndex] != ENGAGEMENT_SIDE::DEFENDER) continue;

				world_army_leave_current_engagement(world, armies.guids[armyIndex]);
			}
		}

		bool fighting = !over && engagements.fighting_active[engagementIndex];
		if (fighting)
		{
			engagements.duration[engagementIndex]++;

			// Both sides roll before anyone takes casualties (attacker first).
			for (ui8 sideIndex = 0; sideIndex < (ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT; sideIndex++)
			{
				ENGAGEMENT_SIDE side = (ENGAGEMENT_SIDE)sideIndex;
				engagements.side_damage(side, engagementIndex) = engagement_roll_side_damage(world, engagements.side_troops(side, engagementIndex));
			}
		}

        // Have all involved armies take damage.
        if (fighting)
        {
            for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
            {
                if (!armies.guids[armyIndex].is_valid() || !(armies.engagement[armyIndex] == engagementGuid)) continue;

                // Take the enemy side's damage, in proportion to this army's men of each type (see engagement_group_kills).
                ENGAGEMENT_SIDE side = armies.engagement_side[armyIndex];
                army_troop_count& composition = armies.composition[armyIndex];
                ui64 menBefore = army_total_manpower(composition);

                army_troop_count kills = {};
                engagement_group_kills(world, engagements.side_damage(engagement_opposite_side(side), engagementIndex), sideMen[(ui8)side], composition, kills);

                army_troop_count dead = {};
                world_army_take_casualties(world, armies.guids[armyIndex], kills, dead);

                for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
                {
                    world_engagement_remove_troops(world, engagementGuid, side, (TROOP_TYPE)unitType, dead.count[unitType]);
                }

                // The dead leave their daily upkeep as supplies.
                world_engagement_loot_add(world, engagementGuid, 0, army_daily_consumption(dead));

                if (army_total_manpower(composition) == 0)
                {
                    // A destroyed army drops everything it carries (before it is destroyed: that can end the engagement).
                    world_engagement_loot_add(world, engagementGuid, armies.gold[armyIndex], armies.supplies[armyIndex]);
                    world_destroy_army(world, armies.guids[armyIndex]);
                }

                // Destroying an army can destroy the engagement, check if it has happened.
                if (!engagements.guids[engagementIndex].is_valid()) 
                    break;
            }
        }
        
        // If engagement was destroyed (probably because last army in it was destroyed), stop logic here.
		if (!engagements.guids[engagementIndex].is_valid())
            continue;

        // Siege-specific logic
		// The garrison takes its share of the attacker's damage, like an army.
		if (fighting && !outside && garrisonIndex != 0xFFFF && garrisonMen > 0)
		{
			army_troop_count kills = {};
			army_troop_count dead = {};
			engagement_group_kills(world, engagements.attacker_damage[engagementIndex], defenderMen, world.entities->settlements.garrison[garrisonIndex], kills);

			settlement_garrison_take_casualties(world, garrisonIndex, kills, dead);
			for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
            {
                world_engagement_remove_troops(world, engagementGuid, ENGAGEMENT_SIDE::DEFENDER, (TROOP_TYPE)unitType, dead.count[unitType]);
            }
			world_engagement_loot_add(world, engagementGuid, 0, army_daily_consumption(dead));
		}

		if (over)
		{
			// Winner: the side with men left (and damage to deal, see above). Neither = no winner (loot goes to the local settlement).
			bool attackerWon = attackerMen > 0;
			bool defenderWon = attackerMen == 0 && defenderMen > 0;
			world_engagement_on_finish(world, engagementGuid, attackerWon || defenderWon, attackerWon ? ENGAGEMENT_SIDE::ATTACKER : ENGAGEMENT_SIDE::DEFENDER);

			// Attacker victory in a siege: the settlement changes owner.
			if (attackerWon && isSiege)
				world_settlement_change_owner(world, engagements.main_target[engagementIndex].get_index(), engagements.attacker_commander[engagementIndex]);

			// Releases the armies left in it too.
			world_engagement_end(world, engagementGuid);
		}
	}
}
