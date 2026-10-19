// Army helpers shared between world simulation themes.

#ifndef WORLD_SIM_ARMY_INCLUDED
#define WORLD_SIM_ARMY_INCLUDED

#include "game_common/match/world.h"

static inline ui64 army_total_manpower(const world_entity_armies::comp& composition)
{
	return (ui64)composition.levies + composition.men_at_arms + composition.horsemen + composition.knights;
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
	army_take_manpower(composition.men_at_arms, amount);
	army_take_manpower(composition.horsemen, amount);
	army_take_manpower(composition.knights, amount);
}

// Man count of each unit type of an army, in the order of world_entity_battles::UNIT_TYPES.
static inline void army_unit_counts(const world_entity_armies::comp& composition, ui32 out_counts[world_entity_battles::UNIT_TYPES])
{
	out_counts[0] = composition.levies;
	out_counts[1] = composition.men_at_arms;
	out_counts[2] = composition.horsemen;
	out_counts[3] = composition.knights;
}

// Man count of each unit type of all the warbands of an army, in the order of world_entity_battles::UNIT_TYPES.
static inline void army_total_unit_counts(const match_world_state& world, ui16 army_index, ui32 out_counts[world_entity_battles::UNIT_TYPES])
{
	for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) out_counts[unitType] = 0;

	const world_entity_armies::warband_list& list = world.entities.armies.warbands[army_index];
	for (ui8 slot = 0; slot < list.count; slot++)
	{
		ui32 counts[world_entity_battles::UNIT_TYPES];
		army_unit_counts(world.entities.warbands.compositions[list.indices[slot]], counts);
		for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) out_counts[unitType] += counts[unitType];
	}
}

// Whether any warband of the army is raising (the army then waits in place).
static inline bool army_is_raising(const match_world_state& world, ui16 army_index)
{
	const world_entity_armies::warband_list& list = world.entities.armies.warbands[army_index];
	for (ui8 slot = 0; slot < list.count; slot++)
	{
		if (world.entities.warbands.raising[list.indices[slot]]) return true;
	}
	return false;
}

#endif // WORLD_SIM_ARMY_INCLUDED
