// Army systems: armies reaching their attack targets and the resolution of those attacks. Siege start is resolved here too.

#include "game_common/match/world/world_entities.h"
#include "world_sim_army.h"

#include "../battles/world_sim_battle.h"
#include "../settlements/world_sim_settlement.h"

#include "../world_sim.h"

// Resolves an attack of an army that reached its target. The attacker is destroyed here if it loses.
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

		// Otherwise the settlement raises levies (20% of its population, at least 1), its whole local troop pool joins them and the siege begins.
		ui32& population = settlements.populations[targetIndex];
		ui32 levies = population / 5 > 0 ? population / 5 : 1;
		if (levies > 0xFFFF) levies = 0xFFFF; // Army levies is a ui16.
		population -= levies < population ? levies : population;

		army_troop_count& pool = settlements.local_troops[targetIndex];
		world_entity_armies::single defenders = {};
		defenders.owner = settlements.owners[targetIndex];
		defenders.location = settlements.locations[targetIndex];
		defenders.composition = { (ui16)levies, pool.men_at_arms, pool.horsemen, pool.knights };
		defenders.raised_troops = defenders.composition;
		pool = {};
		entity_guid defenderArmy = world_spawn_army(world, defenders);

		world_entity_battles::state start = {};
		start.location = (armies.locations[attackerIndex] + settlements.locations[targetIndex]) / 2; // Start battle at midpoint between beligerent armies.
		start.siege_settlement = target;
		start.siege_owner = attackerOwner;
		start.siege_defenders = defenderArmy;
		entity_guid battle = world_spawn_battle(world, start);
		army_battle_join(world, battle, attacker, 0);
		army_battle_join(world, battle, defenderArmy, 1);
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

// Men of each pooled type (men at arms, horsemen, knights) a raising army draws from its home settlement per tick. Levies draw 1 per tick from population.
constexpr ui16 RAISE_POOL_DRAW_PER_TICK = 3;

// Moves men from a settlement pool to an army count, up to the target, the pool content and the per-tick limit.
static void raise_draw_from_pool(ui16& army_count, ui16 target, ui16& pool)
{
	ui16 wanted = army_count < target ? target - army_count : 0;
	ui16 drawn = wanted < pool ? wanted : pool;
	if (drawn > RAISE_POOL_DRAW_PER_TICK) drawn = RAISE_POOL_DRAW_PER_TICK;

	army_count += drawn;
	pool -= drawn;
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
		if (!world_entity_is_valid(world, home) || settlements.owners[home.get_index()] != armies.owners[armyIndex])
		{
			armies.raising[armyIndex] = 0;
			continue;
		}

		// Still walking to the raise location.
		if (!(armies.locations[armyIndex] == armies.movements[armyIndex].destination)) continue;

		ui16 homeIndex = home.get_index();
		army_troop_count& composition = armies.compositions[armyIndex];
		const army_troop_count& target = armies.raised_troops[armyIndex];
		army_troop_count& pool = settlements.local_troops[homeIndex];

		// Levies come out of the population, one per tick, never leaving less than the minimum.
		ui32& population = settlements.populations[homeIndex];
		if (composition.levies < target.levies && population > RAISE_MIN_POPULATION_LEFT)
		{
			composition.levies++;
			population--;
		}

		raise_draw_from_pool(composition.men_at_arms, target.men_at_arms, pool.men_at_arms);
		raise_draw_from_pool(composition.horsemen, target.horsemen, pool.horsemen);
		raise_draw_from_pool(composition.knights, target.knights, pool.knights);

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
