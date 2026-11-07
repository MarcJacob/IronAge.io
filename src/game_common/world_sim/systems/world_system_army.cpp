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

	// Retinue men leave the settlement's population, standing army men stay in it as raised.
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

