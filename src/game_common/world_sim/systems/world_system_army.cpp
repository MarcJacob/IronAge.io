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
		world_entity_battles& battles = world.entities->battles;
		for (ui32 battleIndex = 0; battleIndex < battles.max_count; battleIndex++)
		{
			if (battles.guids[battleIndex].is_valid() && battles.states[battleIndex].siege_settlement == target)
			{
				army_battle_join(world, battles.guids[battleIndex], attacker, 0);
				return;
			}
		}

        // @TODO(Marc): Create temp army for garrison defending the settlement ?
        // Adapt battle system so settlement garrisons can join directly ?
        // For now all sieges will immediately be won by attackers.

		world_entity_battles::state start = {};
		start.location = (armies.locations[attackerIndex] + settlements.locations[targetIndex]) / 2; // Start battle at midpoint between beligerent armies.
		start.siege_settlement = target;
		start.siege_owner = attackerOwner;
		entity_guid battle = world_spawn_battle(world, start);
		army_battle_join(world, battle, attacker, 0);
		break;
	}
	case ENTITY_TYPE::ARMY:
	{
		entity_guid battle = armies.battles[targetIndex];
		if (battle.is_valid())
		{
			// The target is already fighting: join its battle on the side against it.
			army_battle_join(world, battle, attacker, armies.battle_sides[targetIndex] ^ 1);
		}
		else
		{
			// Both armies start a new battle halfway between them, the attacker on side 0. The battle never moves afterwards.
			world_entity_battles::state start = {};
			start.location = (armies.locations[attackerIndex] + armies.locations[targetIndex]) / 2;
			battle = world_spawn_battle(world, start);
			army_battle_join(world, battle, attacker, 0);
			army_battle_join(world, battle, target, 1);
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

        // @TODO(Marc): Deduct unemployed / peasant population ?
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
