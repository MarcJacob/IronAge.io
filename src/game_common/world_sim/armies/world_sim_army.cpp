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

void world_disband_army(match_world_state& world, entity_guid army, entity_guid settlement)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	const army_troop_count& men = world.entities->armies.compositions[army.get_index()];

	ui16 settlementIndex = settlement.get_index();
	world.entities->settlements.populations[settlementIndex] += men.levies;

	// Pooled types go back to the local pool, clamped to ui16.
	army_troop_count& pool = world.entities->settlements.local_troops[settlementIndex];
	ui32 returned[3] = { (ui32)pool.men_at_arms + men.men_at_arms, (ui32)pool.horsemen + men.horsemen, (ui32)pool.knights + men.knights };
	pool.men_at_arms = (ui16)(returned[0] > 0xFFFF ? 0xFFFF : returned[0]);
	pool.horsemen = (ui16)(returned[1] > 0xFFFF ? 0xFFFF : returned[1]);
	pool.knights = (ui16)(returned[2] > 0xFFFF ? 0xFFFF : returned[2]);
	world_destroy_army(world, army);
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
