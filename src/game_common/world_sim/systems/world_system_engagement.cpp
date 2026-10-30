// Engagement systems: joining engagements and resolving engagement rounds. Siege outcomes are resolved here too.
// @TODO(Marc): Move to engagements folder.

#include "core/math/random.h"
#include "game_common/match/world/world_state.h"

#include "../world_sim.h"
#include "../entities/world_sim_entity_settlement.h"
#include "../entities/world_sim_entity_engagement.h"
#include "../entities/world_sim_entity_army.h"

#include "../world_movement.h"

// @TODO(Marc): Move this to army-specific file
void army_engagement_join(match_world_state& world, entity_guid engagement, entity_guid army, ui8 side)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();

	ui32 counts[world_entity_engagements::UNIT_TYPES];
	army_unit_counts(armies.compositions[armyIndex], counts);
	for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++)
	{
		world.entities->engagements.states[engagement.get_index()].side(side).men[unitType] += counts[unitType];
	}

	armies.engagements[armyIndex] = engagement;
	armies.engagement_sides[armyIndex] = side;
	// The army's order goes on after the engagement (it is not worked on while pinned). An attack is over: the army stays at the engagement.
	armies.orders[armyIndex].engaged = INVALID_ENTITY_GUID;
	if (armies.orders[armyIndex].type == ARMY_ORDER_ATTACK)
		army_order_station(armies, armyIndex, world.entities->engagements.states[engagement.get_index()].location);
	world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
	world_engagement_update_commanders(world, engagement);
}

// Engagements resolve one round every this many world ticks (1 = every tick). Engagement duration counts rounds.
constexpr ui32 ENGAGEMENT_TICK_INTERVAL = 20;

// Per unit type stats from the game parameters, in world_entity_engagements::UNIT_TYPES order (levies, men at arms, horsemen, knights).
struct engagement_unit_stats { ui32 damage; ui32 defense; ui32 health; };
static constexpr engagement_unit_stats ENGAGEMENT_UNIT_STATS[world_entity_engagements::UNIT_TYPES] = {
	{ GAME_PARAMETERS.damage_levy, GAME_PARAMETERS.defense_levy, GAME_PARAMETERS.health_levy },
	{ GAME_PARAMETERS.damage_men_at_arms, GAME_PARAMETERS.defense_men_at_arms, GAME_PARAMETERS.health_men_at_arms },
	{ GAME_PARAMETERS.damage_horseman, GAME_PARAMETERS.defense_horseman, GAME_PARAMETERS.health_horseman },
	{ GAME_PARAMETERS.damage_knight, GAME_PARAMETERS.defense_knight, GAME_PARAMETERS.health_knight },
};

// Damage points a side deals this round, before the defences of the other side: its men's total damage x a random percent in [min, max] (rounded up).
// One RNG roll.
static ui32 engagement_roll_side_damage(match_world_state& world, const ui32 men[world_entity_engagements::UNIT_TYPES])
{
	ui32 minPercent = GAME_PARAMETERS.engagement_damage_min_percent;
	ui32 maxPercent = ia_max(GAME_PARAMETERS.engagement_damage_max_percent, minPercent);
	ui32 percent = world.rand_gen.next_range<ui32>(minPercent, maxPercent + 1);
	if (percent > maxPercent) percent = maxPercent; // The range function may land on its max.

	ui64 base = 0;
	for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++) base += (ui64)men[unitType] * ENGAGEMENT_UNIT_STATS[unitType].damage;
	return (ui32)ia_min((base * percent + 99) / 100, (ui64)0xFFFFFFFF);
}

// Men killed in a group (an army or the garrison) of a side that deals with the damage of the other side. The group takes its men's share of the damage
// (sideDamage x men of the type / sideMen, rounded up), so damage is spread over armies and unit types in proportion to their men.
// Per unit type present, in order: a defence roll (reduces the damage by 0 .. men x defense), then, if the damage left does not kill a whole number of men,
// a roll for one more man (probability remainder / health). So up to two RNG rolls per unit type.
static void engagement_group_kills(match_world_state& world, ui64 sideDamage, ui64 sideMen, const ui32 men[world_entity_engagements::UNIT_TYPES], ui32 out_kills[world_entity_engagements::UNIT_TYPES])
{
	for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++)
	{
		out_kills[unitType] = 0;
		if (men[unitType] == 0) continue;

		const engagement_unit_stats& stats = ENGAGEMENT_UNIT_STATS[unitType];
		ui64 aimed = (sideDamage * men[unitType] + sideMen - 1) / sideMen;

		ui32 defence = (ui32)ia_min((ui64)men[unitType] * stats.defense, (ui64)1000000000);
		ui32 reduction = world.rand_gen.next_range<ui32>(0, defence + 1);
		if (reduction > defence) reduction = defence;
		ui64 left = aimed > reduction ? aimed - reduction : 0;

		ui32 health = ia_max(stats.health, (ui32)1);
		ui64 kills = left / health;
		ui32 remainder = (ui32)(left % health);
		if (remainder > 0 && world.rand_gen.next_range<ui32>(0, health) < remainder) kills++;
		out_kills[unitType] = (ui32)ia_min(kills, (ui64)men[unitType]);
	}
}

// @TODO(Marc): Post separation of engagement code from army code, move this to engagement entity folder.
void world_system_engagements(match_world_state& world)
{
	if (world.age % ENGAGEMENT_TICK_INTERVAL != 0) return;

	world_entity_engagements& engagements = world.entities->engagements;
	world_entity_armies& armies = world.entities->armies;

	for (ui32 engagementIndex = 0; engagementIndex < engagements.max_count; engagementIndex++)
	{
		if (!engagements.guids[engagementIndex].is_valid()) continue;

		world_entity_engagements::state& engagement = engagements.states[engagementIndex];

		// Siege: the besieged settlement's garrison is part of the defender side (unless the settlement is already the attackers'). The sides are recounted from their
		// armies and the garrison, which can change between rounds (training, desertion).
		bool besiegingSettlement = engagement.kind == world_entity_engagements::KIND_SIEGE && world_entity_is_valid(world, engagement.main_target);
		ui16 garrisonIndex = 0xFFFF;
		ui64 garrisonMen = 0;
		if (besiegingSettlement)
		{
			for (ui8 side = world_entity_engagements::SIDE_ATTACKER; side <= world_entity_engagements::SIDE_DEFENDER; side++)
			{
				for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++) engagement.side(side).men[unitType] = 0;
			}
			for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
			{
				if (!armies.guids[armyIndex].is_valid() || !(armies.engagements[armyIndex] == engagements.guids[engagementIndex])) continue;

				ui32 counts[world_entity_engagements::UNIT_TYPES];
				army_unit_counts(armies.compositions[armyIndex], counts);
				for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++) engagement.side(armies.engagement_sides[armyIndex]).men[unitType] += counts[unitType];
			}

			ui16 settlementIndex = engagement.main_target.get_index();
			// Not during an outside clash: the garrison (and any army inside) stays out of it.
			if (world.entities->settlements.owners[settlementIndex] != engagement.attacker.commander && engagement.phase != world_entity_engagements::PHASE_OUTSIDE)
			{
				garrisonIndex = settlementIndex;
				ui32 counts[world_entity_engagements::UNIT_TYPES];
				army_unit_counts(world.entities->settlements.garrison[settlementIndex], counts);
				for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++)
				{
					engagement.defender.men[unitType] += counts[unitType];
					garrisonMen += counts[unitType];
				}
			}
		}

		// Men per side at the start of the tick. Armies take damage in proportion to their share of it.
		ui64 sideMen[2] = {}; // Indexed by side (world_entity_engagements::SIDE_*).
		for (ui8 side = world_entity_engagements::SIDE_ATTACKER; side <= world_entity_engagements::SIDE_DEFENDER; side++)
		{
			for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++) sideMen[side] += engagement.side(side).men[unitType];
		}
		ui64 attackerMen = sideMen[world_entity_engagements::SIDE_ATTACKER];
		ui64 defenderMen = sideMen[world_entity_engagements::SIDE_DEFENDER];

		// Commanders follow the men (before any casualty of this round).
		world_engagement_update_commanders(world, engagements.guids[engagementIndex]);

		// A wiped side means the engagement is over: its armies are already dead, the others are released below.
		// Sieges: defender side wiped = conquest by the attacker commander (below); attacker side wiped = siege lifted.
		// A siege whose settlement is gone is over too (nothing left to besiege).
		// An outside clash is the exception: its defender side is only the relief armies, and losing them sends the siege back to besieging (below).
		bool outside = engagement.kind == world_entity_engagements::KIND_SIEGE && engagement.phase == world_entity_engagements::PHASE_OUTSIDE;
		bool over = attackerMen == 0 || (defenderMen == 0 && !outside) || (engagement.kind == world_entity_engagements::KIND_SIEGE && !besiegingSettlement);

		// Relief armies wiped (attacker side won the outside clash): the siege goes on, idle again.
		if (!over && outside && defenderMen == 0)
		{
			engagement.phase = world_entity_engagements::PHASE_BESIEGING;
			engagement.phase_tick = world.age;
			engagement.duration = 0;
		}

		// An idle siege (besieging) only checks for a wiped side: no damage rounds until the assault begins.
		bool fighting = !over && engagement.phase != world_entity_engagements::PHASE_BESIEGING;

		if (fighting)
		{
			engagement.duration++;
			// Both sides roll before anyone takes casualties (attacker first).
			for (ui8 side = world_entity_engagements::SIDE_ATTACKER; side <= world_entity_engagements::SIDE_DEFENDER; side++)
			{
				engagement.side(side).damage = engagement_roll_side_damage(world, engagement.side(side).men);
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
			ui8 side = armies.engagement_sides[armyIndex];
			army_troop_count& composition = armies.compositions[armyIndex];
			ui64 menBefore = army_total_manpower(composition);

			ui32 counts[world_entity_engagements::UNIT_TYPES];
			ui32 kills[world_entity_engagements::UNIT_TYPES];
			army_unit_counts(composition, counts);
			engagement_group_kills(world, engagement.side(world_entity_engagements::opposite_side(side)).damage, sideMen[side], counts, kills);

			ui32 dead[world_entity_engagements::UNIT_TYPES];
			world_army_take_casualties(world, armies.guids[armyIndex], kills, dead);
			for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++) engagement.side(side).men[unitType] -= dead[unitType];

			// A raising army that had no men is just empty (still waiting for its men), not wiped out.
			if (army_total_manpower(composition) == 0 && !(armies.raising[armyIndex] && menBefore == 0)) world_destroy_army(world, armies.guids[armyIndex]);

			// Destroying the last attacking army of a siege ends it (engagement gone, armies released).
			if (!engagements.guids[engagementIndex].is_valid()) break;
		}
		if (!engagements.guids[engagementIndex].is_valid()) continue;

		// The garrison takes its share of the attacker's damage, like an army. Levy deaths kill population. Survivors stay in the garrison.
		if (fighting && !outside && garrisonIndex != 0xFFFF && garrisonMen > 0)
		{
			ui32 counts[world_entity_engagements::UNIT_TYPES];
			ui32 kills[world_entity_engagements::UNIT_TYPES];
			army_unit_counts(world.entities->settlements.garrison[garrisonIndex], counts);
			engagement_group_kills(world, engagement.attacker.damage, defenderMen, counts, kills);

			ui32 dead[world_entity_engagements::UNIT_TYPES];
			settlement_garrison_take_casualties(world, garrisonIndex, kills, dead);
			for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++) engagement.defender.men[unitType] -= ia_min(dead[unitType], engagement.defender.men[unitType]);
		}

		// Attackers wiping the defenders of a siege take the settlement.
		if (over && attackerMen > 0 && besiegingSettlement)
		{
			world_settlement_change_owner(world, engagement.main_target.get_index(), engagement.attacker.commander);
		}

		if (over) world_destroy_engagement(world, engagements.guids[engagementIndex]);
	}
}
