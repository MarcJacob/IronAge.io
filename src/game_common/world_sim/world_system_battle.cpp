// Battle systems: joining battles and resolving battle rounds. Siege outcomes are resolved here too.

#include "core/math/random.h"
#include "game_common/match/world.h"
#include "world_sim.h"
#include "world_sim_army.h"
#include "world_sim_battle.h"

void battle_join(match_world_state& world, entity_guid battle, entity_guid army, ui8 side)
{
	world_entity_armies& armies = world.entities.armies;
	ui16 armyIndex = army.get_index();

	// The men of all the army's warbands join the side.
	ui32 counts[world_entity_battles::UNIT_TYPES];
	army_total_unit_counts(world, armyIndex, counts);
	for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++)
	{
		world.entities.battles.states[battle.get_index()].men[side][unitType] += counts[unitType];
	}

	armies.battles[armyIndex] = battle;
	armies.battle_sides[armyIndex] = side;
	armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
	armies.merging[armyIndex] = 0;
	world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
}

// Each side's damage per tick is this many percent of its men at most.
constexpr ui32 BATTLE_MAX_DAMAGE_PERCENT = 10;

// Battles resolve one round every this many world ticks (1 = every tick). Battle duration counts rounds.
constexpr ui32 BATTLE_TICK_INTERVAL = 20;

void world_system_battles(match_world_state& world)
{
	if (world.age % BATTLE_TICK_INTERVAL != 0) return;

	world_entity_battles& battles = world.entities.battles;
	world_entity_armies& armies = world.entities.armies;
	world_entity_warbands& warbands = world.entities.warbands;

	for (ui32 battleIndex = 0; battleIndex < battles.max_count; battleIndex++)
	{
		if (!battles.guids[battleIndex].is_valid()) continue;

		world_entity_battles::state& battle = battles.states[battleIndex];

		// Men per side at the start of the tick. Warbands take damage in proportion to their share of it.
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

		// Over: the armies still in the battle are released.
		for (ui32 armyIndex = 0; over && armyIndex < armies.max_count; armyIndex++)
		{
			if (!armies.guids[armyIndex].is_valid() || !(armies.battles[armyIndex] == battles.guids[battleIndex])) continue;

			armies.battles[armyIndex] = INVALID_ENTITY_GUID;

			// Defenders winning a siege: the levy army goes back to the settlement's population.
			if (sideMen[1] > 0 && armies.guids[armyIndex] == battle.siege_defenders && world_entity_is_valid(world, battle.siege_settlement))
			{
				world_disband_army(world, armies.guids[armyIndex], battle.siege_settlement);
			}
		}

		// Otherwise each warband of the battle's armies takes the enemy side's damage, proportionally to its share of its side (rounded up).
		for (ui32 warbandIndex = 0; !over && warbandIndex < warbands.max_count; warbandIndex++)
		{
			if (!warbands.guids[warbandIndex].is_valid()) continue;
			ui16 armyIndex = warbands.armies[warbandIndex].get_index();
			if (!(armies.battles[armyIndex] == battles.guids[battleIndex])) continue;

			ui8 side = armies.battle_sides[armyIndex];
			world_entity_warbands::comp& composition = warbands.compositions[warbandIndex];
			ui64 taken = ((ui64)battle.damage[side ^ 1] * army_total_manpower(composition) + sideMen[side] - 1) / sideMen[side];

			ui32 before[world_entity_battles::UNIT_TYPES], after[world_entity_battles::UNIT_TYPES];
			army_unit_counts(composition, before);
			army_remove_manpower(composition, taken);
			army_unit_counts(composition, after);
			for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) battle.men[side][unitType] -= before[unitType] - after[unitType];

			// The dead were still counted in the home settlement: they are gone from it too.
			world_entity_warbands::comp lost = { (ui16)(before[0] - after[0]), (ui16)(before[1] - after[1]), (ui16)(before[2] - after[2]), (ui16)(before[3] - after[3]) };
			world_warband_remove_men_from_home(world, (ui16)warbandIndex, lost);

			// A raising warband that took no damage is just empty (still waiting for its men), not wiped out.
			// Its army goes with its last warband (see world_destroy_warband).
			if (army_total_manpower(composition) == 0 && !(warbands.raising[warbandIndex] && taken == 0)) world_destroy_warband(world, warbands.guids[warbandIndex]);
		}

		// Attackers wiping the defenders of a siege take the settlement.
		if (over && sideMen[0] > 0 && battle.siege_settlement.is_valid() && world_entity_is_valid(world, battle.siege_settlement))
		{
			world.entities.settlements.owners[battle.siege_settlement.get_index()] = battle.siege_owner;
		}

		if (over) world_destroy_battle(world, battles.guids[battleIndex]);
	}
}
