// Battle systems: joining battles and resolving battle rounds. Siege outcomes are resolved here too.
// @TODO(Marc): Move to battles folder.

#include "core/math/random.h"
#include "game_common/match/world/world_state.h"

#include "../world_sim.h"
#include "../entities/world_sim_entity_settlement.h"
#include "../entities/world_sim_entity_battle.h"
#include "../entities/world_sim_entity_army.h"

#include "../world_movement.h"

// @TODO(Marc): Move this to army-specific file
void army_battle_join(match_world_state& world, entity_guid battle, entity_guid army, ui8 side)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = army.get_index();

	ui32 counts[world_entity_battles::UNIT_TYPES];
	army_unit_counts(armies.compositions[armyIndex], counts);
	for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++)
	{
		world.entities->battles.states[battle.get_index()].side(side).men[unitType] += counts[unitType];
	}

	armies.battles[armyIndex] = battle;
	armies.battle_sides[armyIndex] = side;
	armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
	world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
}

// Each side's damage per round (every BATTLE_TICK_INTERVAL ticks) is this many percent of its men at most.
constexpr ui32 BATTLE_MAX_DAMAGE_PERCENT = 10;

// Battles resolve one round every this many world ticks (1 = every tick). Battle duration counts rounds.
constexpr ui32 BATTLE_TICK_INTERVAL = 20;

// @TODO(Marc): Post separation of battle code from army code, move this to battle entity folder.
void world_system_battles(match_world_state& world)
{
	if (world.age % BATTLE_TICK_INTERVAL != 0) return;

	world_entity_battles& battles = world.entities->battles;
	world_entity_armies& armies = world.entities->armies;

	for (ui32 battleIndex = 0; battleIndex < battles.max_count; battleIndex++)
	{
		if (!battles.guids[battleIndex].is_valid()) continue;

		world_entity_battles::state& battle = battles.states[battleIndex];

		// Siege: the besieged settlement's garrison is part of the defender side (unless the settlement is already the attackers'). The sides are recounted from their
		// armies and the garrison, which can change between rounds (training, desertion).
		bool besiegingSettlement = battle.kind == world_entity_battles::KIND_SIEGE && world_entity_is_valid(world, battle.main_target);
		ui16 garrisonIndex = 0xFFFF;
		ui64 garrisonMen = 0;
		if (besiegingSettlement)
		{
			for (ui8 side = world_entity_battles::SIDE_ATTACKER; side <= world_entity_battles::SIDE_DEFENDER; side++)
			{
				for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) battle.side(side).men[unitType] = 0;
			}
			for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
			{
				if (!armies.guids[armyIndex].is_valid() || !(armies.battles[armyIndex] == battles.guids[battleIndex])) continue;

				ui32 counts[world_entity_battles::UNIT_TYPES];
				army_unit_counts(armies.compositions[armyIndex], counts);
				for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) battle.side(armies.battle_sides[armyIndex]).men[unitType] += counts[unitType];
			}

			ui16 settlementIndex = battle.main_target.get_index();
			if (world.entities->settlements.owners[settlementIndex] != battle.attacker.commander)
			{
				garrisonIndex = settlementIndex;
				ui32 counts[world_entity_battles::UNIT_TYPES];
				army_unit_counts(world.entities->settlements.garrison[settlementIndex], counts);
				for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++)
				{
					battle.defender.men[unitType] += counts[unitType];
					garrisonMen += counts[unitType];
				}
			}
		}

		// Men per side at the start of the tick. Armies take damage in proportion to their share of it.
		ui64 sideMen[2] = {}; // Indexed by side (world_entity_battles::SIDE_*).
		for (ui8 side = world_entity_battles::SIDE_ATTACKER; side <= world_entity_battles::SIDE_DEFENDER; side++)
		{
			for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) sideMen[side] += battle.side(side).men[unitType];
		}
		ui64 attackerMen = sideMen[world_entity_battles::SIDE_ATTACKER];
		ui64 defenderMen = sideMen[world_entity_battles::SIDE_DEFENDER];

		// A wiped side means the battle is over: its armies are already dead, the others are released below.
		bool over = attackerMen == 0 || defenderMen == 0;

		if (!over)
		{
			battle.duration++;
			for (ui8 side = world_entity_battles::SIDE_ATTACKER; side <= world_entity_battles::SIDE_DEFENDER; side++)
			{
				ui32 percent = world.rand_gen.next_range<ui32>(1, BATTLE_MAX_DAMAGE_PERCENT + 1);
				if (percent > BATTLE_MAX_DAMAGE_PERCENT) percent = BATTLE_MAX_DAMAGE_PERCENT; // The range function may land on its max.
				battle.side(side).damage = (ui32)((sideMen[side] * percent + 99) / 100); // Rounded up so a fight always costs something.
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
			army_troop_count& composition = armies.compositions[armyIndex];
			ui64 taken = ((ui64)battle.side(world_entity_battles::opposite_side(side)).damage * army_total_manpower(composition) + sideMen[side] - 1) / sideMen[side];

			ui32 dead[world_entity_battles::UNIT_TYPES];
			world_army_take_casualties(world, armies.guids[armyIndex], taken, dead);
			for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) battle.side(side).men[unitType] -= dead[unitType];

			// A raising army that took no damage is just empty (still waiting for its men), not wiped out.
			if (army_total_manpower(composition) == 0 && !(armies.raising[armyIndex] && taken == 0)) world_destroy_army(world, armies.guids[armyIndex]);
		}

		// The garrison takes its share of the attacker's damage, like an army (rounded up). Levy deaths kill population. Survivors stay in the garrison.
		if (!over && garrisonIndex != 0xFFFF && garrisonMen > 0)
		{
			ui64 taken = ((ui64)battle.attacker.damage * garrisonMen + defenderMen - 1) / defenderMen;
			ui32 dead[world_entity_battles::UNIT_TYPES];
			settlement_garrison_take_casualties(world, garrisonIndex, taken, dead);
			for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) battle.defender.men[unitType] -= ia_min(dead[unitType], battle.defender.men[unitType]);
		}

		// Attackers wiping the defenders of a siege take the settlement.
		if (over && attackerMen > 0 && besiegingSettlement)
		{
			world_settlement_change_owner(world, battle.main_target.get_index(), battle.attacker.commander);
		}

		if (over) world_destroy_battle(world, battles.guids[battleIndex]);
	}
}
