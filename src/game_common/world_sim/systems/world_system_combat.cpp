// Army systems: armies reaching their attack targets and the resolution of those attacks. Siege start is resolved here too.

#include "game_common/match/world/world_state.h"

#include "../entities/world_sim_entity_army.h"

#include "../entities/world_sim_entity_engagement.h"
#include "../entities/world_sim_entity_settlement.h"

// Resolves an attack of an army that reached its target entity (called by the order logic, see world_system_army_standing_orders.cpp): starts or joins a siege / engagement,
// or takes an undefended settlement.
void world_army_attack_entity(match_world_state& world, entity_guid attacker, entity_guid target)
{
	world_entity_settlements& settlements = world.entities->settlements;
	world_entity_armies& armies = world.entities->armies;

	ui16 attackerIndex = attacker.get_index();
	ui16 targetIndex = target.get_index();
	match_player_id attackerOwner = armies.owner[attackerIndex];

	// Joining engagements requires a free army with men (see world_army_join_engagement).
	if (armies.engagement[attackerIndex].is_valid() || army_total_manpower(armies.composition[attackerIndex]) == 0) return;

	// An army that cannot inflict damage (only wagons...) attacks nothing: no siege, no battle, no join, no instant conquest.
	if (troops_base_damage(armies.composition[attackerIndex]) == 0) return;

	switch (target.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
	{
		if (settlements.owner[targetIndex] == attackerOwner) break;

		// Join the siege of this settlement if there is one (and it is joinable for the attacker, see world_army_join_engagement).
		entity_guid siege = settlements.siege[targetIndex];
		if (world_entity_is_valid(world, siege))
		{
			world_army_join_engagement(world, attacker, siege);
			return;
		}

		// Nobody defends it (no garrison, and no defender army can be in a new siege): taken at once.
		if (army_total_manpower(settlements.garrison[targetIndex]) == 0)
		{
			world_army_conquest_loot(world, attacker, targetIndex);
			world_settlement_change_owner(world, targetIndex, attackerOwner);
			break;
		}

		world_engagement_start_siege(world, attacker, target);
		break;
	}
	case ENTITY_TYPE::ARMY:
	{
		entity_guid engagement = armies.engagement[targetIndex];
		if (engagement.is_valid())
		{
			// The target is part of an engagement: same as reaching the engagement (joined if joinable, on the attacker's own side, see world_army_join_engagement).
			world_army_join_engagement(world, attacker, engagement);
		}
		else
		{
			world_engagement_start_field_battle(world, attacker, target);
		}
		break;
	}
	case ENTITY_TYPE::ENGAGEMENT:
		world_army_join_engagement(world, attacker, target);
		break;
    case ENTITY_TYPE::CARAVAN:
        world_army_loot_caravan(world, attacker, target);
        break;
	default:
		break;
	}
}

// Moves men of a type from a settlement pool (home) to an army (and the settlement's raised count), up to the target, the pool content and the per-tick limit
// (GAME_PARAMETERS.raise_pool_draw_per_tick, per type).
static void raise_draw_from_pool(match_world_state& world, ui16 army_index, ui16 settlement_index, TROOP_TYPE troop_type, ui16 army_count, ui16 target)
{
	ui16 wanted = army_count < target ? target - army_count : 0;
	if (wanted > GAME_PARAMETERS.raise_pool_draw_per_tick) wanted = GAME_PARAMETERS.raise_pool_draw_per_tick;

	// Retinue men leave the settlement's population, campaign army men stay in it as raised.
	ui16 drawn = world.entities->armies.is_retinue[army_index] ? settlement_raise_retinue_soldiers(world, settlement_index, troop_type, wanted)
		: settlement_raise_soldiers(world, settlement_index, troop_type, wanted);
	army_add_troops(world, army_index, troop_type, drawn);
}

void world_system_armies_raising(match_world_state& world)
{
	world_entity_armies& armies = world.entities->armies;
	world_entity_settlements& settlements = world.entities->settlements;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !armies.raising[armyIndex]) continue;
		if (armies.engagement[armyIndex].is_valid()) continue; // Suspended, resumes after the engagement.

		// Raising ends if the home settlement is gone or changed hands. The army is kept as is.
		entity_guid home = armies.home_settlement[armyIndex];
        const match_player_id& homeOwner = settlements.owner[home.get_index()];
		if (!world_entity_is_valid(world, home) || homeOwner != armies.owner[armyIndex])
		{
			armies.raising[armyIndex] = 0;
			army_enforce_carry_capacity(world, (ui16)armyIndex);
			if (army_total_manpower(armies.composition[armyIndex]) == 0) // Nothing was raised.
			{
				army_homecoming(world, (ui16)armyIndex);
				world_destroy_army(world, armies.guids[armyIndex]);
			}
			continue;
		}

		// Still walking to the raise location.
		if (!(armies.location[armyIndex] == armies.movement[armyIndex].destination)) continue;

		ui16 homeIndex = home.get_index();
		army_troop_count& composition = armies.composition[armyIndex];
		const army_troop_count& target = armies.raised_troops[armyIndex];

        // Levies come from the garrison like the others; each one raised also kills one population (settlement_raise_soldiers).
        raise_draw_from_pool(world, armyIndex, homeIndex, TROOP_TYPE::LEVIES, composition.levies, target.levies);
		raise_draw_from_pool(world, armyIndex, homeIndex, TROOP_TYPE::MEN_AT_ARMS, composition.men_at_arms, target.men_at_arms);
		raise_draw_from_pool(world, armyIndex, homeIndex, TROOP_TYPE::HORSEMEN, composition.horsemen, target.horsemen);
		raise_draw_from_pool(world, armyIndex, homeIndex, TROOP_TYPE::KNIGHTS, composition.knights, target.knights);
		raise_draw_from_pool(world, armyIndex, homeIndex, TROOP_TYPE::WAGONS, composition.wagons, target.wagons); // No pool: limited by the wagons max (settlement_raise_soldiers).

		if (composition.levies >= target.levies && composition.men_at_arms >= target.men_at_arms
			&& composition.horsemen >= target.horsemen && composition.knights >= target.knights && composition.wagons >= target.wagons)
		{
			armies.raising[armyIndex] = 0;
		}
	}
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

// @TODO(Marc): This function has gotten bloated. We could probably factor some of this logic out, not just for the sake of making the function smaller,
// but to more clearly identify and configure the "sub systems", perhaps even split this into multiple separate systems.
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
