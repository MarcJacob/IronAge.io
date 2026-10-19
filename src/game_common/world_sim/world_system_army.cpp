// Army systems: armies reaching their attack targets and the resolution of those attacks. Attacking a settlement starts or joins its siege.

#include "game_common/match/world.h"
#include "world_sim.h"
#include "world_sim_army.h"
#include "world_sim_battle.h"
#include "world_sim_population.h"

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

		// Join the siege of this settlement if there is one (whoever started it), otherwise start one. No room for either: the army just stops.
		entity_guid siege = world_settlement_siege(world, target);
		if (!siege.is_valid())
		{
			if (world.entities.sieges.active_count >= world.entities.sieges.max_count) break;

			world_entity_sieges::state start = {};
			start.location = settlements.locations[targetIndex];
			start.settlement = target;
			start.commander = attackerOwner;
			start.defender = settlements.owners[targetIndex];
			siege = world_spawn_siege(world, start);
		}
		world_siege_add_army(world, siege, attacker);
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

// Men of each pooled type (men at arms, horsemen, knights) a raising army draws from its home settlement per tick. Levies draw 1 per tick from population.
constexpr ui16 RAISE_POOL_DRAW_PER_TICK = 3;

// Raises a warband count towards its target, up to the per-tick limit. The men stay counted in the settlement pool, so the warband can only
// claim men the pool holds beyond what it already has (only one warband per settlement for now, see SETTLEMENT_MAX_RAISED_ARMIES).
static void raise_draw_from_pool(ui16& warband_count, ui16 target, ui16 pool)
{
	ui16 wanted = warband_count < target ? target - warband_count : 0;
	ui16 available = pool > warband_count ? pool - warband_count : 0;
	ui16 drawn = wanted < available ? wanted : available;
	if (drawn > RAISE_POOL_DRAW_PER_TICK) drawn = RAISE_POOL_DRAW_PER_TICK;

	warband_count += drawn;
}

void world_system_armies_raising(match_world_state& world)
{
	world_entity_warbands& warbands = world.entities.warbands;
	world_entity_settlements& settlements = world.entities.settlements;

	for (ui32 warbandIndex = 0; warbandIndex < warbands.max_count; warbandIndex++)
	{
		if (!warbands.guids[warbandIndex].is_valid() || !warbands.raising[warbandIndex]) continue;
		if (world.entities.armies.battles[warbands.armies[warbandIndex].get_index()].is_valid()) continue; // Suspended, resumes after the battle.

		// Raising ends if the home settlement is gone or changed hands. The warband is kept as is.
		entity_guid home = warbands.home_settlements[warbandIndex];
		if (!world_entity_is_valid(world, home) || settlements.owners[home.get_index()] != warbands.owners[warbandIndex])
		{
			warbands.raising[warbandIndex] = 0;
			continue;
		}

		ui16 homeIndex = home.get_index();
		world_entity_warbands::comp& composition = warbands.compositions[warbandIndex];
		const world_entity_warbands::comp& target = warbands.raise_targets[warbandIndex];
		const settlement_troops& pool = settlements.local_troops[homeIndex];

		// Levies are claimed from the population (which keeps counting them), one per tick, never leaving less than the minimum unclaimed.
		ui32 population = settlements.populations[homeIndex];
		if (composition.levies < target.levies && population > (ui32)composition.levies + RAISE_MIN_POPULATION_LEFT)
		{
			composition.levies++;
		}

		raise_draw_from_pool(composition.men_at_arms, target.men_at_arms, pool.men_at_arms);
		raise_draw_from_pool(composition.horsemen, target.horsemen, pool.horsemen);
		raise_draw_from_pool(composition.knights, target.knights, pool.knights);

		if (composition.levies >= target.levies && composition.men_at_arms >= target.men_at_arms
			&& composition.horsemen >= target.horsemen && composition.knights >= target.knights)
		{
			warbands.raising[warbandIndex] = 0;
		}
	}
}

void world_system_armies_target_reached(match_world_state& world)
{
    world_entity_armies& armies = world.entities.armies;

	// Armies that got close enough to their attack target resolve the attack, which ends the chase.
	// Armies following an own army join its battles, and merge into it once close enough if ordered to.
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !armies.action_targets[armyIndex].is_valid()) continue;
		if (armies.battles[armyIndex].is_valid()) continue; // Already fighting.
		if (armies.sieges[armyIndex].is_valid()) continue; // Pinned by its siege: acts once out of it.
		if (army_is_raising(world, (ui16)armyIndex)) continue; // Acts once raised.

		// An earlier resolution this tick may have destroyed the target.
		entity_guid target = armies.action_targets[armyIndex];
		if (!world_entity_is_valid(world, target)) continue; // Handled by the chase code next tick.

		bool inReach = vec2_dist_squared(armies.locations[armyIndex], world_get_entity_location(world, target)) <= ATTACK_REACH_TILES * ATTACK_REACH_TILES;

		if (target.get_type() == ENTITY_TYPE::ARMY && armies.owners[target.get_index()] == armies.owners[armyIndex])
		{
			ui16 followedIndex = target.get_index();
			if (armies.battles[followedIndex].is_valid())
			{
				// The followed army is fighting: join its side, wherever it is. An accompanying army keeps following it afterwards, a merge order is dropped.
				bool accompanying = !armies.merging[armyIndex];
				battle_join(world, armies.battles[followedIndex], armies.guids[armyIndex], armies.battle_sides[followedIndex]);
				if (accompanying) armies.action_targets[armyIndex] = target;
				continue;
			}

			if (!armies.merging[armyIndex] || !inReach) continue; // Accompanying: the movement decision keeps it close.

			// Merge (destroys this army). Dropped if the warbands no longer fit: the army stops where it is.
			armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
			armies.merging[armyIndex] = 0;
			world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
			world_army_merge(world, armies.guids[armyIndex], target);
			continue;
		}

		if (!inReach) continue;

		armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
		world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
		world_resolve_attack(world, armies.guids[armyIndex], target);
	}
}
