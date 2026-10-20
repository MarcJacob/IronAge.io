#include "world_sim_army.h"
#include "../world_sim.h"

entity_guid world_spawn_army(match_world_state& world, const world_entity_armies::single& start_state)
{
	ui16 freeIndex = 0;
	while (freeIndex < world.entities->armies.max_count && world.entities->armies.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities->armies.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::ARMY, freeIndex, world.age % 0x100); // 8-bit extra field.

	world.entities->armies.guids[freeIndex] = newID;
	world.entities->armies.active_count++;

	// Apply start state.
	world.entities->armies.owners[freeIndex] = start_state.owner;
	world.entities->armies.locations[freeIndex] = start_state.location;
	world.entities->armies.movements[freeIndex] = start_state.movement;
	world.entities->armies.compositions[freeIndex] = start_state.composition;
	world.entities->armies.action_targets[freeIndex] = start_state.action_target;
	world.entities->armies.raised_troops[freeIndex] = start_state.raised_troops;
	world.entities->armies.home_settlements[freeIndex] = start_state.home_settlement;
	world.entities->armies.raising[freeIndex] = start_state.raising;

    // Init default values if they weren't specified by start state.
    SET_DEFAULT_VAL(world.entities->armies.movements[freeIndex].travel_speed, 4);
    SET_DEFAULT_VAL_STRUCT(world.entities->armies.movements[freeIndex].destination, start_state.location);
    world.entities->armies.movements[freeIndex].move_target = start_state.location;

	// Not in a battle (the slot may have been used before).
	world.entities->armies.battles[freeIndex] = INVALID_ENTITY_GUID;

	return newID;
}

void world_destroy_army(match_world_state& world, entity_guid army)
{
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	world_entity_armies& armies = world.entities->armies;

	armies.guids[army.get_index()] = INVALID_ENTITY_GUID;
	armies.active_count--;
}

// Moves the men of an army from the settlement's raised count back to its home count.
static void disband_return_troops(ui16& raised, ui16& home, ui16 men)
{
	raised -= ia_min(raised, men);
	ui32 returned = (ui32)home + men;
	home = (ui16)(returned > 0xFFFF ? 0xFFFF : returned);
}

void world_disband_army(match_world_state& world, entity_guid army, entity_guid settlement)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	const army_troop_count& men = world.entities->armies.compositions[army.get_index()];

	settlement_population& population = world.entities->settlements.populations[settlement.get_index()];

	// Levies become peasants again.
	population.raised.levies -= ia_min(population.raised.levies, men.levies);
	population.peasants += men.levies;

	// Other types rejoin the local troops (home), clamped to ui16.
	disband_return_troops(population.raised.men_at_arms, population.home.men_at_arms, men.men_at_arms);
	disband_return_troops(population.raised.horsemen, population.home.horsemen, men.horsemen);
	disband_return_troops(population.raised.knights, population.home.knights, men.knights);
	world_destroy_army(world, army);
}

// Removes up to dead men from a raised count and returns how many were removed.
static ui32 casualties_release_raised(ui16& raised, ui32 dead)
{
	ui16 released = (ui16)ia_min((ui32)raised, dead);
	raised -= released;
	return released;
}

void world_army_take_casualties(match_world_state& world, entity_guid army, ui64 amount, ui32 out_dead[world_entity_battles::UNIT_TYPES])
{
	world_entity_armies& armies = world.entities->armies;
	army_troop_count& composition = armies.compositions[army.get_index()];

	ui32 before[world_entity_battles::UNIT_TYPES];
	army_unit_counts(composition, before);
	army_remove_manpower(composition, amount);
	ui32 after[world_entity_battles::UNIT_TYPES];
	army_unit_counts(composition, after);
	for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) out_dead[unitType] = before[unitType] - after[unitType];

	// The dead also leave their home settlement (raised counts and total population). Skipped if the home is gone.
	entity_guid home = armies.home_settlements[army.get_index()];
	if (!home.is_valid() || home.get_type() != ENTITY_TYPE::SETTLEMENT || !world_entity_is_valid(world, home)) return;

	settlement_population& population = world.entities->settlements.populations[home.get_index()];
	ui32 released = casualties_release_raised(population.raised.levies, out_dead[0])
		+ casualties_release_raised(population.raised.men_at_arms, out_dead[1])
		+ casualties_release_raised(population.raised.horsemen, out_dead[2])
		+ casualties_release_raised(population.raised.knights, out_dead[3]);
	population.population_total -= ia_min(population.population_total, released);
}

bool world_entity_query_army(const match_world_state& world, entity_guid entity, world_entity_armies::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::ARMY || !world_entity_is_valid(world, entity)) return false;

	const world_entity_armies& armies = world.entities->armies;
	ui16 i = entity.get_index();

	out_state = {
		.owner = armies.owners[i],
		.location = armies.locations[i],
		.movement = armies.movements[i],
		.composition = armies.compositions[i],
		.raised_troops = armies.raised_troops[i],
		.raising = armies.raising[i],
		.home_settlement = armies.home_settlements[i],
		.action_target = armies.action_targets[i],
		.battle = armies.battles[i],
		.battle_side = armies.battle_sides[i],
	};
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

	return true;
}
