#include "world_sim_entity_campaign_objective.h"
#include "../world_sim.h"

entity_guid world_spawn_campaign_objective(match_world_state& world, CAMPAIGN_OBJECTIVE_TYPE type, entity_guid target, world_location location)
{
	world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;
	ui16 freeIndex = 0;
	while (freeIndex < objectives.max_count && objectives.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	if (freeIndex >= objectives.max_count) return INVALID_ENTITY_GUID;

	entity_guid newID = entity_guid_new(ENTITY_TYPE::CAMPAIGN_OBJECTIVE, freeIndex, world.age % 0x100); // 8-bit extra field.

	world_entity_campaign_objective_view newObjective { objectives, freeIndex };
	newObjective.copy_from(world_entity_campaign_objective {});
	newObjective.type = type;
	newObjective.target = target;
	newObjective.location = location;
	objectives.guids[freeIndex] = newID;
	objectives.active_count++;

	return newID;
}

void world_destroy_campaign_objective(match_world_state& world, entity_guid objective)
{
	ASSERT(world_entity_is_valid(world, objective) && objective.get_type() == ENTITY_TYPE::CAMPAIGN_OBJECTIVE);
	world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;

	ui16 index = objective.get_index();
	world_entity_campaign_objective_view destroyedObjective { objectives, index };
	destroyedObjective.copy_from(world_entity_campaign_objective {});
	objectives.guids[index] = INVALID_ENTITY_GUID;
	objectives.active_count--;
}

entity_guid world_find_campaign_objective(const match_world_state& world, CAMPAIGN_OBJECTIVE_TYPE type, entity_guid target)
{
	const world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;
	for (ui16 index = 0; index < objectives.max_count; index++)
	{
		if (objectives.guids[index].is_valid() && objectives.type[index] == type && objectives.target[index] == target) return objectives.guids[index];
	}
	return INVALID_ENTITY_GUID;
}

bool world_alloc_campaign_objectives(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;

	objectives.max_count = count;

	objectives.guids = memory.alloc<entity_guid>(count);
	if (objectives.guids == nullptr) return false;

	objectives.location = memory.alloc<world_location>(count);
	if (objectives.location == nullptr) return false;

	objectives.type = memory.alloc<CAMPAIGN_OBJECTIVE_TYPE>(count);
	if (objectives.type == nullptr) return false;

	objectives.target = memory.alloc<entity_guid>(count);
	if (objectives.target == nullptr) return false;

	objectives.data = memory.alloc<campaign_objective_data>(count);
	if (objectives.data == nullptr) return false;

	objectives.committed = memory.alloc<ui32>(count);
	if (objectives.committed == nullptr) return false;

	objectives.idle_intervals = memory.alloc<ui16>(count);
	if (objectives.idle_intervals == nullptr) return false;

	return true;
}
