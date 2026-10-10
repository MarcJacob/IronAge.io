#include "core/math/random.h"
#include "game_common/match/world.h"
#include "world_sim.h"

// ATTACK RESOLUTION HELPERS

// An army reaches its attack target when within this many tiles of it.
constexpr i32 ATTACK_REACH_TILES = 5;

static inline ui64 army_total_manpower(const world_entity_armies::comp& composition)
{
	return (ui64)composition.levies + composition.archers + composition.men_at_arms + composition.horsemen + composition.knights;
}

template<typename Count>
static inline void army_take_manpower(Count& count, ui64& remaining)
{
	ui64 taken = remaining < count ? remaining : count;
	count = (Count)(count - taken);
	remaining -= taken;
}

// Removes manpower from an army, cheapest units first.
static inline void army_remove_manpower(world_entity_armies::comp& composition, ui64 amount)
{
	army_take_manpower(composition.levies, amount);
	army_take_manpower(composition.archers, amount);
	army_take_manpower(composition.men_at_arms, amount);
	army_take_manpower(composition.horsemen, amount);
	army_take_manpower(composition.knights, amount);
}

// Effective manpower of an army for a fight, in hundredths: manpower x a random 50% to 150% advantage.
static inline ui64 army_roll_effective_manpower(rand_generator_32 rand_gen, ui64 manpower)
{
	ui32 advantagePercent = rand_gen.next_range<ui32>(50, 151);
	if (advantagePercent > 150) advantagePercent = 150; // The range function may land on its max.
	return manpower * advantagePercent;
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
		settlements.owners[targetIndex] = attackerOwner;
		break;
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
		ui64 attackerManpower = army_total_manpower(armies.compositions[attackerIndex]);
		ui64 defenderManpower = army_total_manpower(armies.compositions[targetIndex]);

		// Roll attacker first, then defender. Manpower is assumed to stay under ~1e8 so the loss computation below can't overflow.
		ui64 attackerEffective = army_roll_effective_manpower(world.rand_gen, attackerManpower);
		ui64 defenderEffective = army_roll_effective_manpower(world.rand_gen, defenderManpower);

		if (attackerEffective == defenderEffective)
		{
			world_destroy_army(world, attacker);
			world_destroy_army(world, target);
		}
		else
		{
			// The winner, whichever side, loses winner_manpower x (loser_eff / winner_eff), rounded up so a fight always costs something.
			// The ratio is below 1 here, but the loss is still capped at the winner's manpower.
			bool attackerWins = attackerEffective > defenderEffective;
			entity_guid winner = attackerWins ? attacker : target;
			entity_guid loser = attackerWins ? target : attacker;
			ui64 winnerManpower = attackerWins ? attackerManpower : defenderManpower;
			ui64 winnerEffective = attackerWins ? attackerEffective : defenderEffective;
			ui64 loserEffective = attackerWins ? defenderEffective : attackerEffective;

			ui64 loss = (winnerManpower * loserEffective + winnerEffective - 1) / winnerEffective;
			if (loss > winnerManpower) loss = winnerManpower;

			world_destroy_army(world, loser);
			army_remove_manpower(armies.compositions[winner.get_index()], loss);
			if (loss == winnerManpower) world_destroy_army(world, winner);
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

		// An earlier resolution this tick may have destroyed the target.
		entity_guid target = armies.action_targets[armyIndex];
		if (!world_entity_is_valid(world, target)) continue; // Handled by the chase code next tick.

		if (vec2_dist_squared(armies.locations[armyIndex], world_get_entity_location(world, target))
			> ATTACK_REACH_TILES * ATTACK_REACH_TILES) continue;

		armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
		armies.movements[armyIndex].move_target = armies.locations[armyIndex];
		world_resolve_attack(world, armies.guids[armyIndex], target);
	}
}
