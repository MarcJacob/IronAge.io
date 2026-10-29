#include "world_sim_entity_army.h"
#include "../world_sim.h"
#include "world_sim_entity_settlement.h"
#include "game_common/match/world/world.h"

entity_guid world_spawn_army(match_world_state& world, const world_entity_armies::single& start_state)
{
	world_entity_armies& armies = world.entities->armies;

	ui16 freeIndex = 0;
	while (freeIndex < armies.max_count && armies.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < armies.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::ARMY, freeIndex, world.age % 0x100); // 8-bit extra field.

	world_entity_armies::view newArmy { armies, freeIndex };
	newArmy.copy_from(start_state);
	newArmy.battle = INVALID_ENTITY_GUID;
	newArmy.desertions = 0;

	// Init default values if they weren't specified by start state.
	SET_DEFAULT_VAL(newArmy.movement.travel_speed, (ui16)GAME_PARAMETERS.army_travel_speed);
	SET_DEFAULT_VAL_STRUCT(newArmy.movement.destination, start_state.location);

	armies.guids[freeIndex] = newID;
	armies.active_count++;

	return newID;
}

void world_destroy_army(match_world_state& world, entity_guid army)
{
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	world_entity_armies& armies = world.entities->armies;

	ui16 index = army.get_index();
	world_entity_army_view destroyedArmy { armies, index };
	destroyedArmy.copy_from(world_entity_army {});
	armies.guids[index] = INVALID_ENTITY_GUID;
	armies.active_count--;
}

void world_disband_army(match_world_state& world, entity_guid army, entity_guid settlement)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	army_troop_count men = world.entities->armies.compositions[army.get_index()];

	// Levies become peasants again, the other types rejoin the local troops (home).
	for (ui8 troopType = 0; troopType < TROOP_TYPE_COUNT; troopType++)
	{
		settlement_release_raised(world, settlement.get_index(), troopType, troop_count_field(men, troopType));
	}
	world_destroy_army(world, army);
}

void world_army_take_casualties(match_world_state& world, entity_guid army, ui64 amount, ui32 out_dead[world_entity_battles::UNIT_TYPES])
{
	world_entity_armies& armies = world.entities->armies;

	// Cheapest units first.
	ui64 remaining = amount;
	for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++)
	{
		out_dead[unitType] = army_remove_troops(world, army.get_index(), unitType, (ui16)ia_min(remaining, (ui64)0xFFFF));
		remaining -= out_dead[unitType];
	}

	// The dead also leave their home settlement (raised counts and total population). Skipped if the home is gone.
	entity_guid home = armies.home_settlements[army.get_index()];
	if (!home.is_valid() || home.get_type() != ENTITY_TYPE::SETTLEMENT || !world_entity_is_valid(world, home)
            || world.entities->settlements.owners[home.get_index()] != armies.owners[army.get_index()]) return;

	for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++)
	{
		settlement_remove_soldiers_raised(world, home.get_index(), unitType, (ui16)out_dead[unitType]);
	}
}

ui16 world_army_desertion(match_world_state& world, entity_guid army, ui8 troop_type, ui16 count)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 deserters = army_remove_troops(world, army.get_index(), troop_type, count);
	if (deserters == 0) return 0;

	// An army in a battle no longer counts them among its side's men.
	entity_guid battle = armies.battles[army.get_index()];
	if (battle.is_valid())
	{
		ui32& sideMen = world.entities->battles.states[battle.get_index()].men[armies.battle_sides[army.get_index()]][troop_type];
		sideMen -= ia_min(sideMen, (ui32)deserters);
	}

	// They only count at home if the home settlement still is the owner's. Otherwise they are gone (like casualties).
	entity_guid home = armies.home_settlements[army.get_index()];
	if (home.is_valid() && home.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, home)
		&& world.entities->settlements.owners[home.get_index()] == armies.owners[army.get_index()])
	{
		// Those the settlement cannot take back (full pool) are lost.
		ui16 returned = settlement_release_raised(world, home.get_index(), troop_type, deserters);
		settlement_remove_soldiers_raised(world, home.get_index(), troop_type, deserters - returned);
	}

	// The last man leaving ends the army. The army is invalid after this.
	if (army_total_manpower(armies.compositions[army.get_index()]) == 0) world_destroy_army(world, army);
	return deserters;
}

bool world_entity_query_army(const match_world_state& world, entity_guid entity, world_entity_armies::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::ARMY || !world_entity_is_valid(world, entity)) return false;

	const world_entity_armies& armies = world.entities->armies;
	ui16 i = entity.get_index();

	out_state.copy_from(armies, i);
	out_state.supplies_consumption = army_daily_consumption(armies.compositions[i]);
	out_state.supplies_capacity = army_supply_capacity(world, i);
	return true;
}

bool world_alloc_armies(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_armies& armies = world.entities->armies;

	armies.max_count = count;

	armies.guids = memory.alloc<entity_guid>(count);
	if (armies.guids == nullptr) return false;

	armies.owners = memory.alloc<match_player_id>(count);
	if (armies.owners == nullptr) return false;

	armies.locations = memory.alloc<world_location>(count);
	if (armies.locations == nullptr) return false;

	armies.movements = memory.alloc<world_entity_movement>(count);
	if (armies.movements == nullptr) return false;

	armies.compositions = memory.alloc<army_troop_count>(count);
	if (armies.compositions == nullptr) return false;

	armies.battles = memory.alloc<entity_guid>(count);
	if (armies.battles == nullptr) return false;

	armies.battle_sides = memory.alloc<ui8>(count);
	if (armies.battle_sides == nullptr) return false;

	armies.raised_troops = memory.alloc<army_troop_count>(count);
	if (armies.raised_troops == nullptr) return false;

	armies.raising = memory.alloc<ui8>(count);
	if (armies.raising == nullptr) return false;

	armies.home_settlements = memory.alloc<entity_guid>(count);
	if (armies.home_settlements == nullptr) return false;

	armies.action_targets = memory.alloc<entity_guid>(count);
	if (armies.action_targets == nullptr) return false;

	armies.supplies = memory.alloc<ui32>(count);
	if (armies.supplies == nullptr) return false;

	armies.desertions = memory.alloc<ui16>(count);
	if (armies.desertions == nullptr) return false;

	return true;
}

// What the given soldiers consume per day: sum of count x per-type upkeep rate. Levies are per mille on the levy total, rounded down.
ui32 army_daily_consumption(const army_troop_count& troops)
{
	ui64 consumption = ((ui64)troops.levies * GAME_PARAMETERS.upkeep_levy_per_mille) / 1000
		+ (ui64)troops.men_at_arms * GAME_PARAMETERS.upkeep_men_at_arms
		+ (ui64)troops.horsemen * GAME_PARAMETERS.upkeep_horseman
		+ (ui64)troops.knights * GAME_PARAMETERS.upkeep_knight;
	return (ui32)ia_min(consumption, (ui64)0xFFFFFFFF);
}

// The troops supplies are sized for: the raise targets while the army is raising, its composition after that.
const army_troop_count& army_supply_basis(const match_world_state& world, ui16 army_index)
{
	const world_entity_armies& armies = world.entities->armies;
	return armies.raising[army_index] ? armies.raised_troops[army_index] : armies.compositions[army_index];
}

// Supplies the army refills up to: army_supply_capacity_days of the consumption of its supply basis.
ui32 army_supply_capacity(const match_world_state& world, ui16 army_index)
{
	return (ui32)ia_min((ui64)army_daily_consumption(army_supply_basis(world, army_index)) * GAME_PARAMETERS.army_supply_capacity_days, (ui64)0xFFFFFFFF);
}
