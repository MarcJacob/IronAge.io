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

// Man count of each unit type of an army, in the order of world_entity_battles::UNIT_TYPES.
static inline void army_unit_counts(const world_entity_armies::comp& composition, ui32 out_counts[world_entity_battles::UNIT_TYPES])
{
	out_counts[0] = composition.levies;
	out_counts[1] = composition.archers;
	out_counts[2] = composition.men_at_arms;
	out_counts[3] = composition.horsemen;
	out_counts[4] = composition.knights;
}

// Makes an army fight in a battle on the given side: its men are added to the side and it stops moving.
static void battle_join(match_world_state& world, entity_guid battle, entity_guid army, ui8 side)
{
	world_entity_armies& armies = world.entities.armies;
	ui16 armyIndex = army.get_index();

	ui32 counts[world_entity_battles::UNIT_TYPES];
	army_unit_counts(armies.compositions[armyIndex], counts);
	for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++)
	{
		world.entities.battles.states[battle.get_index()].men[side][unitType] += counts[unitType];
	}

	armies.battles[armyIndex] = battle;
	armies.battle_sides[armyIndex] = side;
	armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
	world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
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
		entity_guid battle = armies.battles[targetIndex];
		if (battle.is_valid())
		{
			// The target is already fighting: join its battle against it.
			battle_join(world, battle, attacker, armies.battle_sides[targetIndex] ^ 1);
		}
		else
		{
			// Both armies start a new battle at the target's location, the attacker on side 0.
			world_entity_battles::state start = {};
			start.location = armies.locations[targetIndex];
			start.side_owners[0] = attackerOwner;
			start.side_owners[1] = armies.owners[targetIndex];
			battle = world_spawn_battle(world, start);
			battle_join(world, battle, attacker, 0);
			battle_join(world, battle, target, 1);
		}
		break;
	}
	case ENTITY_TYPE::BATTLE:
	{
		// Join the side of the attacker's owner, or side 1 if it owns neither.
		ui8 side = world.entities.battles.states[targetIndex].side_owners[0] == attackerOwner ? 0 : 1;
		battle_join(world, target, attacker, side);
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

// BATTLES

// Each side's damage per tick is this many percent of its men at most.
constexpr ui32 BATTLE_MAX_DAMAGE_PERCENT = 10;

void world_system_battles(match_world_state& world)
{
	world_entity_battles& battles = world.entities.battles;
	world_entity_armies& armies = world.entities.armies;

	for (ui32 battleIndex = 0; battleIndex < battles.max_count; battleIndex++)
	{
		if (!battles.guids[battleIndex].is_valid()) continue;

		world_entity_battles::state& battle = battles.states[battleIndex];

		// Men per side at the start of the tick. Armies take damage in proportion to their share of it.
		ui64 sideMen[2] = {};
		for (ui8 side = 0; side < 2; side++)
		{
			for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) sideMen[side] += battle.men[side][unitType];
		}

		// A wiped side means the battle is over: its armies are already dead, the others are released below.
		bool over = sideMen[0] == 0 || sideMen[1] == 0;

		if (!over)
		{
			battle.duration++;
			for (ui8 side = 0; side < 2; side++)
			{
				ui32 percent = world.rand_gen.next_range<ui32>(1, BATTLE_MAX_DAMAGE_PERCENT + 1);
				if (percent > BATTLE_MAX_DAMAGE_PERCENT) percent = BATTLE_MAX_DAMAGE_PERCENT; // The range function may land on its max.
				battle.damage[side] = (ui32)((sideMen[side] * percent + 99) / 100); // Rounded up so a fight always costs something.
			}
		}

		for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			if (!armies.guids[armyIndex].is_valid() || !(armies.battles[armyIndex] == battles.guids[battleIndex])) continue;

			if (over)
			{
				armies.battles[armyIndex] = INVALID_ENTITY_GUID;
				continue;
			}

			// Take the enemy side's damage, proportionally to this army's share of its side (rounded up).
			ui8 side = armies.battle_sides[armyIndex];
			world_entity_armies::comp& composition = armies.compositions[armyIndex];
			ui64 taken = ((ui64)battle.damage[side ^ 1] * army_total_manpower(composition) + sideMen[side] - 1) / sideMen[side];

			ui32 before[world_entity_battles::UNIT_TYPES], after[world_entity_battles::UNIT_TYPES];
			army_unit_counts(composition, before);
			army_remove_manpower(composition, taken);
			army_unit_counts(composition, after);
			for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) battle.men[side][unitType] -= before[unitType] - after[unitType];

			if (army_total_manpower(composition) == 0) world_destroy_army(world, armies.guids[armyIndex]);
		}

		if (over) world_destroy_battle(world, battles.guids[battleIndex]);
	}
}
