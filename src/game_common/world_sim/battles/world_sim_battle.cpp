#include "world_sim_battle.h"
#include "../world_sim.h"

entity_guid world_spawn_battle(match_world_state& world, const world_entity_battles::state& start_state)
{
	ui16 freeIndex = 0;
	while (freeIndex < world.entities->battles.max_count && world.entities->battles.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities->battles.max_count);

	entity_guid newID = entity_guid_new(ENTITY_TYPE::BATTLE, freeIndex, world.age % 0x100); // 8-bit extra field.

	world.entities->battles.guids[freeIndex] = newID;
	world.entities->battles.active_count++;
	world.entities->battles.states[freeIndex] = start_state;

	return newID;
}

void world_destroy_battle(match_world_state& world, entity_guid battle)
{
	ASSERT(world_entity_is_valid(world, battle) && battle.get_type() == ENTITY_TYPE::BATTLE);
	world_entity_battles& battles = world.entities->battles;

	battles.guids[battle.get_index()] = INVALID_ENTITY_GUID;
	battles.active_count--;
}

bool world_entity_query_battle(const match_world_state& world, entity_guid entity, world_entity_battles::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::BATTLE || !world_entity_is_valid(world, entity)) return false;

	out_state = world.entities->battles.states[entity.get_index()];
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
