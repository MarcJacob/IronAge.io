// Army symbols / helpers shared between world simulation themes.

#ifndef WORLD_SIM_ARMY_INCLUDED
#define WORLD_SIM_ARMY_INCLUDED

#include "game_common/match/game_parameters.h"
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

// Troop types, in the order of world_entity_battles::UNIT_TYPES (cheapest first).
constexpr ui8 TROOP_TYPE_LEVIES = 0;
constexpr ui8 TROOP_TYPE_MEN_AT_ARMS = 1;
constexpr ui8 TROOP_TYPE_HORSEMEN = 2;
constexpr ui8 TROOP_TYPE_KNIGHTS = 3;
constexpr ui8 TROOP_TYPE_COUNT = 4;

static inline ui16& troop_count_field(army_troop_count& troops, ui8 troop_type)
{
	switch (troop_type)
	{
	case TROOP_TYPE_LEVIES: return troops.levies;
	case TROOP_TYPE_MEN_AT_ARMS: return troops.men_at_arms;
	case TROOP_TYPE_HORSEMEN: return troops.horsemen;
	default: return troops.knights;
	}
}

// ARMY PRIMITIVES
// Army composition changes. Take the army's index. They only touch the army: the home settlement's counts and the player stats are the caller's business.

// Adds men of a type to the army's composition (clamped to the ui16 count). Returns how many were added.
static inline ui16 army_add_troops(match_world_state& world, ui16 army_index, ui8 troop_type, ui16 count)
{
	ui16& troops = troop_count_field(world.entities->armies.compositions[army_index], troop_type);
	ui16 added = ia_min(count, (ui16)(0xFFFF - troops));
	troops += added;
	return added;
}

// Removes up to count men of a type from the army's composition. Returns how many were removed.
static inline ui16 army_remove_troops(match_world_state& world, ui16 army_index, ui8 troop_type, ui16 count)
{
	ui16& troops = troop_count_field(world.entities->armies.compositions[army_index], troop_type);
	ui16 removed = ia_min(count, troops);
	troops -= removed;
	return removed;
}

// Man count of each unit type of an army, in the order of world_entity_battles::UNIT_TYPES.
static inline void army_unit_counts(const army_troop_count& composition, ui32 out_counts[world_entity_battles::UNIT_TYPES])
{
	out_counts[0] = composition.levies;
	out_counts[1] = composition.men_at_arms;
	out_counts[2] = composition.horsemen;
	out_counts[3] = composition.knights;
}

// SUPPLIES

// What the given soldiers consume per day: sum of count x per-type upkeep rate. Levies are per mille on the levy total, rounded down.
static inline ui32 army_daily_consumption(const army_troop_count& troops)
{
	ui64 consumption = ((ui64)troops.levies * GAME_PARAMETERS.upkeep_levy_per_mille) / 1000
		+ (ui64)troops.men_at_arms * GAME_PARAMETERS.upkeep_men_at_arms
		+ (ui64)troops.horsemen * GAME_PARAMETERS.upkeep_horseman
		+ (ui64)troops.knights * GAME_PARAMETERS.upkeep_knight;
	return (ui32)ia_min(consumption, (ui64)0xFFFFFFFF);
}

// The troops supplies are sized for: the raise targets while the army is raising, its composition after that.
static inline const army_troop_count& army_supply_basis(const match_world_state& world, ui16 army_index)
{
	const world_entity_armies& armies = world.entities->armies;
	return armies.raising[army_index] ? armies.raised_troops[army_index] : armies.compositions[army_index];
}

// Supplies the army refills up to: army_supply_capacity_days of the consumption of its supply basis.
static inline ui32 army_supply_capacity(const match_world_state& world, ui16 army_index)
{
	return (ui32)ia_min((ui64)army_daily_consumption(army_supply_basis(world, army_index)) * GAME_PARAMETERS.army_supply_capacity_days, (ui64)0xFFFFFFFF);
}

// Removes up to count deserters of a type from the army. They go back to the army's home settlement (levies as peasants, the others in the local pool),
// or vanish if the home is invalid, changed hands, or has no room in its pool. Returns how many left the army.
// @TODO(Marc): The further from home the army is, the less likely deserters should be to make it back.
ui16 world_army_desertion(match_world_state& world, entity_guid army, ui8 troop_type, ui16 count);

// Kills up to amount men of an army (cheapest first), returns the dead per unit type, and removes them from the home settlement's raised counts and total population (skipped if the home is invalid or changed hands).
void world_army_take_casualties(match_world_state& world, entity_guid army, ui64 amount, ui32 out_dead[world_entity_battles::UNIT_TYPES]);

void army_battle_join(match_world_state& world, entity_guid battle, entity_guid army, ui8 side);

#endif // WORLD_SIM_ARMY_INCLUDED
