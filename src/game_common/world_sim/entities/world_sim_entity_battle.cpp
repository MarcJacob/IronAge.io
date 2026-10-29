#include "world_sim_entity_battle.h"

entity_guid world_spawn_battle(match_world_state& world, const world_entity_battles::state& start_state)
{
	world_entity_battles& battles = world.entities->battles;
	ui16 freeIndex = 0;
	while (freeIndex < battles.max_count && battles.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < battles.max_count);

	entity_guid newID = entity_guid_new(ENTITY_TYPE::BATTLE, freeIndex, world.age % 0x100); // 8-bit extra field.

	world_entity_battles::view newBattle { battles, freeIndex };
	newBattle.copy_from(start_state);
	newBattle.data.phase_tick = world.age;
	battles.guids[freeIndex] = newID;
	battles.active_count++;

	// A siege is recorded on the besieged settlement (cleared in world_destroy_battle).
	if (start_state.kind == world_entity_battles::KIND_SIEGE)
	{
		ASSERT(start_state.main_target.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, start_state.main_target));
		world.entities->settlements.sieges[start_state.main_target.get_index()] = newID;
	}

	return newID;
}

void world_destroy_battle(match_world_state& world, entity_guid battle)
{
	ASSERT(world_entity_is_valid(world, battle) && battle.get_type() == ENTITY_TYPE::BATTLE);
	world_entity_battles& battles = world.entities->battles;

	ui16 index = battle.get_index();

	// Clear the besieged settlement's siege (it may be gone already, or under another siege by now).
	const world_entity_battles::state& ended = battles.states[index];
	if (ended.kind == world_entity_battles::KIND_SIEGE && ended.main_target.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, ended.main_target))
	{
		entity_guid& siege = world.entities->settlements.sieges[ended.main_target.get_index()];
		if (siege == battle) siege = INVALID_ENTITY_GUID;
	}

	world_entity_battle_view destroyedBattle { battles, index };
	destroyedBattle.copy_from(world_entity_battle {});
	battles.guids[index] = INVALID_ENTITY_GUID;
	battles.active_count--;
}

bool world_entity_query_battle(const match_world_state& world, entity_guid entity, world_entity_battles::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::BATTLE || !world_entity_is_valid(world, entity)) return false;

	const world_entity_battles& battles = world.entities->battles;
	ui16 index = entity.get_index();
	out_state.copy_from(battles, index);
	return true;
}

bool world_alloc_battles(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_battles& battles = world.entities->battles;

	battles.max_count = count;

	battles.guids = memory.alloc<entity_guid>(count);
	if (battles.guids == nullptr) return false;

	battles.states = memory.alloc<world_entity_battles::state>(count);
	if (battles.states == nullptr) return false;

	return true;
}
