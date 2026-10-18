// Army systems: armies reaching their attack targets and the resolution of those attacks. Siege start is resolved here too.

#include "game_common/match/world.h"
#include "world_sim.h"
#include "world_sim_battle.h"

// An army reaches its attack target when within this many tiles of it.
constexpr i32 ATTACK_REACH_TILES = 5;

static inline world_location world_location_midpoint(world_location a, world_location b)
{
	return { (ui16)(((ui32)a.x + b.x) / 2), (ui16)(((ui32)a.y + b.y) / 2) };
}

// Resolves an attack of an army that reached its target. The attacker is destroyed here if it loses.
static void world_resolve_attack(match_world_state& world, entity_guid attacker, entity_guid target)
{
	world_entity_settlements& settlements = world.entities.settlements;
	world_entity_caravans& caravans = world.entities.caravans;
	world_entity_armies& armies = world.entities.armies;

	ui16 attackerIndex = attacker.get_index();
	ui16 targetIndex = target.get_index();
	match_player_id attackerOwner = armies.owners[attackerIndex];

	switch (target.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
	{
		if (settlements.owners[targetIndex] == attackerOwner) break;

		// Join the siege of this settlement if there is one.
		world_entity_battles& battles = world.entities.battles;
		for (ui32 battleIndex = 0; battleIndex < battles.max_count; battleIndex++)
		{
			if (battles.guids[battleIndex].is_valid() && battles.states[battleIndex].siege_settlement == target)
			{
				battle_join(world, battles.guids[battleIndex], attacker, 0);
				return;
			}
		}

		// Otherwise the settlement raises levies (10% of its population, at least 1) and the siege begins.
		ui32& population = settlements.populations[targetIndex];
		ui32 levies = population / 10 > 0 ? population / 10 : 1;
		if (levies > 0xFFFF) levies = 0xFFFF; // Army levies is a ui16.
		population -= levies < population ? levies : population;

		world_entity_armies::single defenders = {};
		defenders.owner = settlements.owners[targetIndex];
		defenders.location = settlements.locations[targetIndex];
		defenders.composition.levies = (ui16)levies;
		entity_guid defenderArmy = world_spawn_army(world, defenders);

		world_entity_battles::state start = {};
		start.location = world_location_midpoint(armies.locations[attackerIndex], settlements.locations[targetIndex]);
		start.siege_settlement = target;
		start.siege_owner = attackerOwner;
		start.siege_defenders = defenderArmy;
		entity_guid battle = world_spawn_battle(world, start);
		battle_join(world, battle, attacker, 0);
		battle_join(world, battle, defenderArmy, 1);
		break;
	}
	case ENTITY_TYPE::CARAVAN:
	{
		// The caravan is redirected to the closest settlement of the attacker. Without one, it is left alone.
		i32 bestDist = 0;
		entity_guid bestSettlement = INVALID_ENTITY_GUID;
		for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
		{
			if (!settlements.guids[settlementIndex].is_valid() || settlements.owners[settlementIndex] != attackerOwner) continue;

			i32 dist = vec2_dist_squared(caravans.locations[targetIndex], settlements.locations[settlementIndex]);
			if (!bestSettlement.is_valid() || dist < bestDist)
			{
				bestDist = dist;
				bestSettlement = settlements.guids[settlementIndex];
			}
		}
		if (bestSettlement.is_valid()) caravans.dest_settlements[targetIndex] = bestSettlement;
		break;
	}
	case ENTITY_TYPE::ARMY:
	{
		entity_guid battle = armies.battles[targetIndex];
		if (battle.is_valid())
		{
			// The target is already fighting: join its battle on the side against it.
			battle_join(world, battle, attacker, armies.battle_sides[targetIndex] ^ 1);
		}
		else
		{
			// Both armies start a new battle halfway between them, the attacker on side 0. The battle never moves afterwards.
			world_entity_battles::state start = {};
			start.location = world_location_midpoint(armies.locations[attackerIndex], armies.locations[targetIndex]);
			battle = world_spawn_battle(world, start);
			battle_join(world, battle, attacker, 0);
			battle_join(world, battle, target, 1);
		}
		break;
	}
	default:
		break;
	}
}

void world_system_armies_target_reached(match_world_state& world)
{
    world_entity_armies& armies = world.entities.armies;

	// Armies that got close enough to their attack target resolve the attack, which ends the chase.
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !armies.action_targets[armyIndex].is_valid()) continue;
		if (armies.battles[armyIndex].is_valid()) continue; // Already fighting.

		// An earlier resolution this tick may have destroyed the target.
		entity_guid target = armies.action_targets[armyIndex];
		if (!world_entity_is_valid(world, target)) continue; // Handled by the chase code next tick.

		if (vec2_dist_squared(armies.locations[armyIndex], world_get_entity_location(world, target))
			> ATTACK_REACH_TILES * ATTACK_REACH_TILES) continue;

		armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
		world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
		world_resolve_attack(world, armies.guids[armyIndex], target);
	}
}
