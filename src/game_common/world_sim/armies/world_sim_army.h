// Army symbols / helpers shared between world simulation themes.

#ifndef WORLD_SIM_ARMY_INCLUDED
#define WORLD_SIM_ARMY_INCLUDED

#include "game_common/match/world/world_entities.h"

// An army reaches its attack target when within this many tiles of it.
constexpr i32 ATTACK_REACH_TILES = 5;

// Spawns a new army in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_army(match_world_state& world, const world_entity_armies::single& start_state);
void world_destroy_army(match_world_state& world, entity_guid army);

// Allocates the armies' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_armies(match_world_state& world, mem_arena& memory, ui32 count);

static inline ui64 army_total_manpower(const army_troop_count& composition)
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
static inline void army_remove_manpower(army_troop_count& composition, ui64 amount)
{
	army_take_manpower(composition.levies, amount);
	army_take_manpower(composition.men_at_arms, amount);
	army_take_manpower(composition.horsemen, amount);
	army_take_manpower(composition.knights, amount);
}

// Man count of each unit type of an army, in the order of world_entity_battles::UNIT_TYPES.
static inline void army_unit_counts(const army_troop_count& composition, ui32 out_counts[world_entity_battles::UNIT_TYPES])
{
	out_counts[0] = composition.levies;
	out_counts[1] = composition.men_at_arms;
	out_counts[2] = composition.horsemen;
	out_counts[3] = composition.knights;
}

// Kills up to amount men of an army (cheapest first), returns the dead per unit type, and removes them from the home settlement's raised counts and total population (clamped at 0; skipped if the home is invalid).
void world_army_take_casualties(match_world_state& world, entity_guid army, ui64 amount, ui32 out_dead[world_entity_battles::UNIT_TYPES]);

void army_battle_join(match_world_state& world, entity_guid battle, entity_guid army, ui8 side);

#endif // WORLD_SIM_ARMY_INCLUDED
