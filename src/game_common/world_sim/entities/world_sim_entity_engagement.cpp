#include "world_sim_entity_engagement.h"
#include "game_common/match/world/troop_types.h"
#include "world_sim_entity_army.h"
#include "world_sim_entity_settlement.h"
#include "core/math/random.h"
#include "../world_sim.h"
#include "../world_movement.h"

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
	if (start_state.kind == ENGAGEMENT_KIND::SIEGE)
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
	if (ended.kind == ENGAGEMENT_KIND::SIEGE && ended.main_target.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, ended.main_target))
	{
		entity_guid& siege = world.entities->settlements.sieges[ended.main_target.get_index()];
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
		world.entities->engagements.states[engagement.get_index()].side(side).troops.count[unitType] += armies.compositions[armyIndex].count[unitType];
	}

	armies.engagements[armyIndex] = engagement;
	armies.engagement_sides[armyIndex] = side;
	// The army's order is kept and goes on after the engagement (it is not worked on while pinned).
	armies.orders[armyIndex].engaged = INVALID_ENTITY_GUID;
	world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
	world_engagement_update_commanders(world, engagement);
}

bool world_army_pick_engagement_side(const match_world_state& world, entity_guid army, entity_guid engagement, ENGAGEMENT_SIDE& out_side)
{
	const world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();

	PLAYER_DIPLOMATIC_STATUS status = engagement_diplomatic_status(world, engagement, armies.owners[armyIndex]);
	if (status == PLAYER_DIPLOMATIC_STATUS_NEUTRAL) return false;

	const world_entity_engagements::state& state = world.entities->engagements.states[engagement.get_index()];
	// Friendly = join the attackers, hostile = join the defenders.
	out_side = status == PLAYER_DIPLOMATIC_STATUS_FRIENDLY ? ENGAGEMENT_SIDE::ATTACKER : ENGAGEMENT_SIDE::DEFENDER;

	// An army standing on the besieged settlement does not take the defender side of its siege.
	if (state.kind == ENGAGEMENT_KIND::SIEGE && out_side == ENGAGEMENT_SIDE::DEFENDER && world_entity_is_valid(world, state.main_target)
		&& armies.locations[armyIndex] == world.entities->settlements.locations[state.main_target.get_index()]) return false;

	return true;
}

bool world_army_join_engagement(match_world_state& world, entity_guid army, entity_guid engagement)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	ASSERT(!armies.engagements[armyIndex].is_valid()); // Callers make sure the army is free. Empty armies should not be passed either.
	ASSERT(army_total_manpower(armies.compositions[armyIndex]) > 0);

	ENGAGEMENT_SIDE side;
	if (!world_army_pick_engagement_side(world, army, engagement, side)) return false;

	world_entity_engagements::state& state = world.entities->engagements.states[engagement.get_index()];
	world_army_join_engagement(world, army, engagement, side);
	if (state.kind == ENGAGEMENT_KIND::SIEGE && side == ENGAGEMENT_SIDE::DEFENDER && state.phase == ENGAGEMENT_PHASE::BESIEGING)
	{
		state.phase = ENGAGEMENT_PHASE::OUTSIDE;
		state.phase_tick = world.age;
	}
	return true;
}

bool world_engagement_can_be_left(const match_world_state& world, entity_guid engagement, world_location location, ENGAGEMENT_SIDE side)
{
	const world_entity_engagements::state& state = world.entities->engagements.states[engagement.get_index()];

	// Far enough away.
	float leaveRadius = (float)(engagement_join_radius(world, engagement) * GAME_PARAMETERS.engagement_leave_radius_multiplier);
	if (vec2_dist_squared(location, state.location) > leaveRadius * leaveRadius) return true;

	// Otherwise only the attackers of an idle siege: nothing fights there, they just walk out.
	return state.kind == ENGAGEMENT_KIND::SIEGE && state.phase == ENGAGEMENT_PHASE::BESIEGING && side == ENGAGEMENT_SIDE::ATTACKER;
}

bool world_army_can_leave_current_engagement(const match_world_state& world, entity_guid army)
{
	const world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();
	if (!armies.engagements[armyIndex].is_valid()) return false;

	return world_engagement_can_be_left(world, armies.engagements[armyIndex], armies.locations[armyIndex], armies.engagement_sides[armyIndex]);
}

void world_army_leave_current_engagement(match_world_state& world, entity_guid army)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();
	entity_guid engagement = armies.engagements[armyIndex];
	if (!engagement.is_valid()) return;

	// Its men leave their side (a siege recounts its sides every round anyway).
	for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
		world_engagement_remove_troops(world, engagement, armies.engagement_sides[armyIndex], (TROOP_TYPE)unitType, armies.compositions[armyIndex].count[unitType]);

	armies.engagements[armyIndex] = INVALID_ENTITY_GUID;
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
	world_entity_engagements::state start = {};
	start.defender.troops = settlements.garrison[settlementIndex];
	start.location = (armies.locations[attackerIndex] + settlements.locations[settlementIndex]) / 2; // Start engagement at midpoint between belligerents.
	start.kind = ENGAGEMENT_KIND::SIEGE;
	start.phase = ENGAGEMENT_PHASE::BESIEGING;
	start.main_target = settlement;
	start.attacker.commander = armies.owners[attackerIndex];
	start.defender.commander = settlements.owners[settlementIndex];
	entity_guid engagement = world_spawn_engagement(world, start);
	world_army_join_engagement(world, attacker, engagement, ENGAGEMENT_SIDE::ATTACKER);
	return engagement;
}

entity_guid world_engagement_start_field_battle(match_world_state& world, entity_guid attacker, entity_guid target)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 attackerIndex = attacker.get_index();
	ui16 targetIndex = target.get_index();

	world_entity_engagements::state start = {};
	start.location = (armies.locations[attackerIndex] + armies.locations[targetIndex]) / 2;
	start.kind = ENGAGEMENT_KIND::FIELD;
	start.phase = ENGAGEMENT_PHASE::ACTIVE;
	start.main_target = target;
	start.attacker.commander = armies.owners[attackerIndex];
	start.defender.commander = armies.owners[targetIndex];
	entity_guid engagement = world_spawn_engagement(world, start);
	world_army_join_engagement(world, attacker, engagement, ENGAGEMENT_SIDE::ATTACKER);
	world_army_join_engagement(world, target, engagement, ENGAGEMENT_SIDE::DEFENDER);
	return engagement;
}

void world_engagement_siege_begin_assault(match_world_state& world, entity_guid engagement)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	world_entity_engagements::state& siege = world.entities->engagements.states[engagement.get_index()];
	ASSERT(siege.kind == ENGAGEMENT_KIND::SIEGE && siege.phase == ENGAGEMENT_PHASE::BESIEGING);

	siege.phase = ENGAGEMENT_PHASE::ASSAULT;
	siege.phase_tick = world.age;
}

void world_engagement_siege_sally_out(match_world_state& world, entity_guid engagement)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	world_entity_engagements::state& siege = world.entities->engagements.states[engagement.get_index()];
	ASSERT(siege.kind == ENGAGEMENT_KIND::SIEGE && (siege.phase == ENGAGEMENT_PHASE::BESIEGING || siege.phase == ENGAGEMENT_PHASE::OUTSIDE));

	// The garrison joins the defender side at the next round (the siege recounts its sides then).
	siege.phase = ENGAGEMENT_PHASE::SALLY;
	siege.phase_tick = world.age;
}

void world_engagement_siege_lift(match_world_state& world, entity_guid engagement)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	ASSERT(world.entities->engagements.states[engagement.get_index()].kind == ENGAGEMENT_KIND::SIEGE);
	ASSERT(world.entities->engagements.states[engagement.get_index()].phase == ENGAGEMENT_PHASE::BESIEGING);

	// Every army freed stands where it is (its order would otherwise take it straight back into a new siege).
	world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !(armies.engagements[armyIndex] == engagement)) continue;

		army_order_station(armies, (ui16)armyIndex, armies.locations[armyIndex]);
		world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
	}

	world_engagement_end(world, engagement);
}

ui16 world_engagement_remove_troops(match_world_state& world, entity_guid engagement, ENGAGEMENT_SIDE side, TROOP_TYPE troop_type, ui16 count)
{
	ASSERT(world_entity_is_valid(world, engagement) && engagement.get_type() == ENTITY_TYPE::ENGAGEMENT);
	ui16& troops = world.entities->engagements.states[engagement.get_index()].side(side).troops.access_troop_count(troop_type);
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
	for (ui8 sideIndex = 0; sideIndex < (ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT; sideIndex++)
	{
		ENGAGEMENT_SIDE side = (ENGAGEMENT_SIDE)sideIndex;
		if (state.kind == ENGAGEMENT_KIND::SIEGE && side == ENGAGEMENT_SIDE::DEFENDER) continue;

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
	if (world.entities->engagements.states[engagement.get_index()].kind != ENGAGEMENT_KIND::SIEGE) return;

	const world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (armies.guids[armyIndex].is_valid() && armies.engagements[armyIndex] == engagement && armies.engagement_sides[armyIndex] == ENGAGEMENT_SIDE::ATTACKER) return;
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
	ui8 rating[(ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT];
	for (ui8 sideIndex = 0; sideIndex < (ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT; sideIndex++)
	{
		PLAYER_DIPLOMATIC_STATUS commanderStatus = world_player_get_diplomatic_status(world, player, state.side((ENGAGEMENT_SIDE)sideIndex).commander);
		rating[sideIndex] = commanderStatus == PLAYER_DIPLOMATIC_STATUS_FRIENDLY ? 2 : (commanderStatus == PLAYER_DIPLOMATIC_STATUS_HOSTILE ? 0 : 1);
	}

	ui8 attackerRating = rating[(ui8)ENGAGEMENT_SIDE::ATTACKER];
	ui8 defenderRating = rating[(ui8)ENGAGEMENT_SIDE::DEFENDER];
	if (attackerRating == defenderRating) return PLAYER_DIPLOMATIC_STATUS_NEUTRAL;
	return attackerRating > defenderRating ? PLAYER_DIPLOMATIC_STATUS_FRIENDLY : PLAYER_DIPLOMATIC_STATUS_HOSTILE;
}

ui32 engagement_join_radius(const match_world_state& world, entity_guid engagement)
{
	const world_entity_engagements::state& state = world.entities->engagements.states[engagement.get_index()];

	ui64 troops = army_total_manpower(state.attacker.troops) + army_total_manpower(state.defender.troops);

	// A siege's defender side holds the garrison (inside the settlement) unless it is an outside clash, or the settlement is the attackers' already: not counted.
	if (state.kind == ENGAGEMENT_KIND::SIEGE && state.phase != ENGAGEMENT_PHASE::OUTSIDE &&world_entity_is_valid(world, state.main_target)
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
			if (!armies.guids[armyIndex].is_valid() || armies.engagements[armyIndex].is_valid()) continue;
			if (army_total_manpower(armies.compositions[armyIndex]) == 0) continue;
			if (vec2_dist_squared(armies.locations[armyIndex], engagements.states[engagementIndex].location) > radius * radius) continue;

			// An army that could leave right away (attacker of an idle siege...) is not aggroed: its order logic joins it if it wants to.
			ENGAGEMENT_SIDE side;
			if (!world_army_pick_engagement_side(world, armies.guids[armyIndex], engagement, side)) continue;
			if (world_engagement_can_be_left(world, engagement, armies.locations[armyIndex], side)) continue;

			world_army_join_engagement(world, armies.guids[armyIndex], engagement);
		}
	}
}

// Damage points a side deals this round, before the defences of the other side: its men's total damage x a random percent in [min, max] (rounded up).
// One RNG roll.
static ui32 engagement_roll_side_damage(match_world_state& world, const army_troop_count& troops)
{
	ui32 minPercent = GAME_PARAMETERS.engagement_damage_min_percent;
	ui32 maxPercent = ia_max(GAME_PARAMETERS.engagement_damage_max_percent, minPercent);
	ui32 percent = world.rand_gen.next_range<ui32>(minPercent, maxPercent + 1);
	if (percent > maxPercent) percent = maxPercent; // The range function may land on its max.

	ui64 base = 0;
	for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
		base += (ui64)troops.count[unitType] * GAME_PARAMETERS.troop_type_stats[unitType].damage;
	return (ui32)ia_min((base * percent + 99) / 100, (ui64)0xFFFFFFFF);
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

		world_entity_engagements::state& engagement = engagements.states[engagementIndex];

		// Siege: the besieged settlement's garrison is part of the defender side (unless the settlement is already the attackers'). The sides are recounted from their
		// armies and the garrison, which can change between rounds (training, desertion).
		bool besiegingSettlement = engagement.kind == ENGAGEMENT_KIND::SIEGE && world_entity_is_valid(world, engagement.main_target);
		ui16 garrisonIndex = 0xFFFF;
		ui64 garrisonMen = 0;
		if (besiegingSettlement)
		{
			engagement.attacker.troops = {};
			engagement.defender.troops = {};
			for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
			{
				if (!armies.guids[armyIndex].is_valid() || !(armies.engagements[armyIndex] == engagements.guids[engagementIndex])) continue;

				army_troop_count& sideTroops = engagement.side(armies.engagement_sides[armyIndex]).troops;
				for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++) sideTroops.count[unitType] += armies.compositions[armyIndex].count[unitType];
			}

			ui16 settlementIndex = engagement.main_target.get_index();

			// Not during an outside clash: the garrison (and any army inside) stays out of it.
			if (world.entities->settlements.owners[settlementIndex] != engagement.attacker.commander && engagement.phase != ENGAGEMENT_PHASE::OUTSIDE)
			{
				garrisonIndex = settlementIndex;
				for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
				{
					ui16 count = world.entities->settlements.garrison[garrisonIndex].count[unitType];
					engagement.defender.troops.count[unitType] += count;
					garrisonMen += count;
				}
			}
		}

		// Men per side at the start of the tick. Armies take damage in proportion to their share of it.
		ui64 sideMen[(ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT] = {}; // Indexed by ENGAGEMENT_SIDE.
		for (ui8 sideIndex = 0; sideIndex < (ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT; sideIndex++) sideMen[sideIndex] = army_total_manpower(engagement.side((ENGAGEMENT_SIDE)sideIndex).troops);
		ui64 attackerMen = sideMen[(ui8)ENGAGEMENT_SIDE::ATTACKER];
		ui64 defenderMen = sideMen[(ui8)ENGAGEMENT_SIDE::DEFENDER];

		// Commanders follow the men (before any casualty of this round).
		world_engagement_update_commanders(world, engagements.guids[engagementIndex]);

		// A wiped side means the engagement is over: its armies are already dead, the others are released below.
		// Sieges: defender side wiped = conquest by the attacker commander (below); attacker side wiped = siege lifted.
		// A siege whose settlement is gone is over too (nothing left to besiege).
		// An outside clash is the exception: its defender side is only the relief armies, and losing them sends the siege back to besieging (below).
		bool outside = engagement.kind == ENGAGEMENT_KIND::SIEGE && engagement.phase == ENGAGEMENT_PHASE::OUTSIDE;
		bool over = attackerMen == 0 || (defenderMen == 0 && !outside) || (engagement.kind == ENGAGEMENT_KIND::SIEGE && !besiegingSettlement);

		// Relief armies wiped (attacker side won the outside clash): the siege goes on, idle again.
		if (!over && outside && defenderMen == 0)
		{
			engagement.phase = ENGAGEMENT_PHASE::BESIEGING;
			engagement.phase_tick = world.age;
			engagement.duration = 0;
		}

		// An idle siege (besieging) only checks for a wiped side: no damage rounds until the assault begins.
		bool fighting = !over && engagement.phase != ENGAGEMENT_PHASE::BESIEGING;

		if (fighting)
		{
			engagement.duration++;
			// Both sides roll before anyone takes casualties (attacker first).
			for (ui8 sideIndex = 0; sideIndex < (ui8)ENGAGEMENT_SIDE::ENGAGEMENT_SIDE_COUNT; sideIndex++)
			{
				world_entity_engagements::side_state& sideState = engagement.side((ENGAGEMENT_SIDE)sideIndex);
				sideState.damage = engagement_roll_side_damage(world, sideState.troops);
			}
		}

		for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			if (!armies.guids[armyIndex].is_valid() || !(armies.engagements[armyIndex] == engagements.guids[engagementIndex])) continue;

			if (over)
			{
				armies.engagements[armyIndex] = INVALID_ENTITY_GUID;
				continue;
			}
			if (!fighting) continue;

			// Take the enemy side's damage, in proportion to this army's men of each type (see engagement_group_kills).
			ENGAGEMENT_SIDE side = armies.engagement_sides[armyIndex];
			army_troop_count& composition = armies.compositions[armyIndex];
			ui64 menBefore = army_total_manpower(composition);

			army_troop_count kills = {};
			engagement_group_kills(world, engagement.side(engagement_opposite_side(side)).damage, sideMen[(ui8)side], composition, kills);

			army_troop_count dead = {};
			world_army_take_casualties(world, armies.guids[armyIndex], kills, dead);
			for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++) world_engagement_remove_troops(world, engagements.guids[engagementIndex], side, (TROOP_TYPE)unitType, dead.count[unitType]);

			// A raising army that had no men is just empty (still waiting for its men), not wiped out.
			if (army_total_manpower(composition) == 0 && !(armies.raising[armyIndex] && menBefore == 0)) world_destroy_army(world, armies.guids[armyIndex]);

			// Destroying the last attacking army of a siege ends it (engagement gone, armies released).
			if (!engagements.guids[engagementIndex].is_valid()) break;
		}
		if (!engagements.guids[engagementIndex].is_valid()) continue;

		// The garrison takes its share of the attacker's damage, like an army. Levy deaths kill population. Survivors stay in the garrison.
		if (fighting && !outside && garrisonIndex != 0xFFFF && garrisonMen > 0)
		{
			army_troop_count kills = {};
			army_troop_count dead = {};
			engagement_group_kills(world, engagement.attacker.damage, defenderMen, world.entities->settlements.garrison[garrisonIndex], kills);

			settlement_garrison_take_casualties(world, garrisonIndex, kills, dead);
			for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++) world_engagement_remove_troops(world, engagements.guids[engagementIndex], ENGAGEMENT_SIDE::DEFENDER, (TROOP_TYPE)unitType, dead.count[unitType]);
		}

		// Attackers wiping the defenders of a siege take the settlement.
		if (over && attackerMen > 0 && besiegingSettlement)
		{
			world_settlement_change_owner(world, engagement.main_target.get_index(), engagement.attacker.commander);
		}

		if (over) world_destroy_engagement(world, engagements.guids[engagementIndex]);
	}
}
