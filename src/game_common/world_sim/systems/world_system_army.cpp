// Army systems: armies reaching their attack targets and the resolution of those attacks. Siege start is resolved here too.

#include "game_common/match/world/world_state.h"

#include "../entities/world_sim_entity_army.h"

#include "../entities/world_sim_entity_battle.h"
#include "../entities/world_sim_entity_settlement.h"

#include "../world_movement.h"

// Resolves an attack of an army that reached its target.
static void world_resolve_attack(match_world_state& world, entity_guid attacker, entity_guid target)
{
	world_entity_settlements& settlements = world.entities->settlements;
	world_entity_armies& armies = world.entities->armies;

	ui16 attackerIndex = attacker.get_index();
	ui16 targetIndex = target.get_index();
	match_player_id attackerOwner = armies.owners[attackerIndex];

	switch (target.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
	{
		if (settlements.owners[targetIndex] == attackerOwner) break;

		// Join the siege of this settlement if there is one.
		entity_guid siege = settlements.sieges[targetIndex];
		if (world_entity_is_valid(world, siege))
		{
			army_battle_join(world, siege, attacker, world_entity_battles::SIDE_ATTACKER);
			return;
		}

		// Nobody defends it (no garrison, and no defender army can be in a new siege): taken at once.
		if (army_total_manpower(settlements.garrison[targetIndex]) == 0)
		{
			world_settlement_change_owner(world, targetIndex, attackerOwner);
			break;
		}

		// The settlement's garrison is the defending side, it is recounted into the battle at every round (see world_system_battles).
		// A siege starts idle (besieging): no damage until the attacker commander begins the assault.
		world_entity_battles::state start = {};
		army_unit_counts(settlements.garrison[targetIndex], start.defender.men);
		start.location = (armies.locations[attackerIndex] + settlements.locations[targetIndex]) / 2; // Start battle at midpoint between beligerent armies.
		start.kind = world_entity_battles::KIND_SIEGE;
		start.phase = world_entity_battles::PHASE_BESIEGING;
		start.main_target = target;
		start.attacker.commander = attackerOwner;
		start.defender.commander = settlements.owners[targetIndex];
		entity_guid battle = world_spawn_battle(world, start);
		army_battle_join(world, battle, attacker, world_entity_battles::SIDE_ATTACKER);
		break;
	}
	case ENTITY_TYPE::ARMY:
	{
		entity_guid battle = armies.battles[targetIndex];
		if (battle.is_valid())
		{
			// The target is already fighting: join its battle on the side against it.
			army_battle_join(world, battle, attacker, world_entity_battles::opposite_side(armies.battle_sides[targetIndex]));
		}
		else
		{
			// Both armies start a new battle halfway between them, the attacker on the attacker side. The battle never moves afterwards.
			world_entity_battles::state start = {};
			start.location = (armies.locations[attackerIndex] + armies.locations[targetIndex]) / 2;
			start.kind = world_entity_battles::KIND_FIELD;
			start.phase = world_entity_battles::PHASE_ACTIVE;
			start.main_target = target;
			start.attacker.commander = attackerOwner;
			start.defender.commander = armies.owners[targetIndex];
			battle = world_spawn_battle(world, start);
			army_battle_join(world, battle, attacker, world_entity_battles::SIDE_ATTACKER);
			army_battle_join(world, battle, target, world_entity_battles::SIDE_DEFENDER);
		}
		break;
	}
	default:
		break;
	}
}

// Men of each pooled type (men at arms, horsemen, knights) a raising army draws from its home settlement per tick.
constexpr ui16 RAISE_POOL_DRAW_PER_TICK = 3;

// Levies a raising army draws from its home settlement's peasants per tick.
constexpr ui16 RAISE_LEVIES_PER_TICK = 2;

// Moves men of a type from a settlement pool (home) to an army (and the settlement's raised count), up to the target, the pool content and the per-tick limit.
static void raise_draw_from_pool(match_world_state& world, ui16 army_index, ui16 settlement_index, ui8 troop_type, ui16 army_count, ui16 target)
{
	ui16 wanted = army_count < target ? target - army_count : 0;
	if (wanted > RAISE_POOL_DRAW_PER_TICK) wanted = RAISE_POOL_DRAW_PER_TICK;

	army_add_troops(world, army_index, troop_type, settlement_raise_soldiers(world, settlement_index, troop_type, wanted));
}

void world_system_armies_raising(match_world_state& world)
{
	world_entity_armies& armies = world.entities->armies;
	world_entity_settlements& settlements = world.entities->settlements;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !armies.raising[armyIndex]) continue;
		if (armies.battles[armyIndex].is_valid()) continue; // Suspended, resumes after the battle.

		// Raising ends if the home settlement is gone or changed hands. The army is kept as is.
		entity_guid home = armies.home_settlements[armyIndex];
        const match_player_id& homeOwner = settlements.owners[home.get_index()];
		if (!world_entity_is_valid(world, home) || homeOwner != armies.owners[armyIndex])
		{
			armies.raising[armyIndex] = 0;
			if (army_total_manpower(armies.compositions[armyIndex]) == 0) world_destroy_army(world, armies.guids[armyIndex]); // Nothing was raised.
			continue;
		}

		// Still walking to the raise location.
		if (!(armies.locations[armyIndex] == armies.movements[armyIndex].destination)) continue;

		ui16 homeIndex = home.get_index();
		army_troop_count& composition = armies.compositions[armyIndex];
		const army_troop_count& target = armies.raised_troops[armyIndex];

        // Levies come from the garrison like the others; each one raised also kills one population (settlement_raise_soldiers).
        raise_draw_from_pool(world, armyIndex, homeIndex, TROOP_TYPE_LEVIES, composition.levies, target.levies);
		raise_draw_from_pool(world, armyIndex, homeIndex, TROOP_TYPE_MEN_AT_ARMS, composition.men_at_arms, target.men_at_arms);
		raise_draw_from_pool(world, armyIndex, homeIndex, TROOP_TYPE_HORSEMEN, composition.horsemen, target.horsemen);
		raise_draw_from_pool(world, armyIndex, homeIndex, TROOP_TYPE_KNIGHTS, composition.knights, target.knights);

		if (composition.levies >= target.levies && composition.men_at_arms >= target.men_at_arms
			&& composition.horsemen >= target.horsemen && composition.knights >= target.knights)
		{
			armies.raising[armyIndex] = 0;
		}
	}
}

void world_system_armies_target_reached(match_world_state& world)
{
    world_entity_armies& armies = world.entities->armies;

	// Armies that got close enough to their attack target resolve the attack, which ends the chase.
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !armies.action_targets[armyIndex].is_valid()) continue;
		if (armies.battles[armyIndex].is_valid()) continue; // Already fighting.

		// An earlier resolution this tick may have destroyed the target.
		entity_guid target = armies.action_targets[armyIndex];
		if (!world_entity_is_valid(world, target)) continue; // Handled by the chase code next tick.

		if (vec2_dist_squared(armies.locations[armyIndex], world_entity_get_location(world, target))
			> ATTACK_REACH_TILES * ATTACK_REACH_TILES) continue;

		armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
		world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
		world_resolve_attack(world, armies.guids[armyIndex], target);
	}
}
