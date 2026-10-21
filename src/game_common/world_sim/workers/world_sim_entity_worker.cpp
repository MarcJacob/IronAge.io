#include "world_sim_entity_worker.h"

entity_guid world_spawn_worker(match_world_state& world, const world_entity_workers::single& start_state)
{
	ASSERT(start_state.work_type < WORK_TYPE::WORK_TYPE_COUNT);
	if (!world_entity_is_valid(world, start_state.origin_entity)) return INVALID_ENTITY_GUID;

    // @TODO(Marc): Similarly to input command, have a validation function for each work type.

	ui16 freeIndex = 0;
	while (freeIndex < world.entities->workers.max_count && world.entities->workers.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities->workers.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::WORKER, freeIndex, world.age % 0x100); // 8-bit extra field.

	world.entities->workers.guids[freeIndex] = newID;
	world.entities->workers.active_count++;

	// Apply start state.
	world.entities->workers.origin_entity[freeIndex] = start_state.origin_entity;
	world.entities->workers.work_types[freeIndex] = start_state.work_type;
	world.entities->workers.work_times[freeIndex] = start_state.work_time;
	world.entities->workers.action_data[freeIndex] = start_state.action_data;

	// Every worker starts at its origin. Moving work types take it from there (start_state.position / movement are not read).
	world_location start = world_entity_get_location(world, start_state.origin_entity);
	world.entities->workers.locations[freeIndex] = start;
	world_entity_movement& movement = world.entities->workers.movements[freeIndex];
	movement = {};
	movement.travel_speed = 4; // Same as armies.
	movement.destination = start;
	movement.move_target = start;
	movement.path_handle = INVALID_PATH_HANDLE;

	return newID;
}

void world_destroy_worker(match_world_state& world, entity_guid worker)
{
	ASSERT(world_entity_is_valid(world, worker) && worker.get_type() == ENTITY_TYPE::WORKER);
	world_entity_workers& workers = world.entities->workers;

	workers.guids[worker.get_index()] = INVALID_ENTITY_GUID;
	workers.active_count--;
}

bool world_entity_query_worker(const match_world_state& world, entity_guid entity, world_entity_workers::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::WORKER || !world_entity_is_valid(world, entity)) return false;

	const world_entity_workers& workers = world.entities->workers;
	ui16 i = entity.get_index();

	out_state = {
		.origin_entity = workers.origin_entity[i],
		.work_type = workers.work_types[i],
		.work_time = workers.work_times[i],
		.action_data = workers.action_data[i],
		.position = workers.locations[i],
		.movement = workers.movements[i],
	};
	return true;
}

bool world_alloc_workers(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_workers& workers = world.entities->workers;

	workers.max_count = count;
	workers.active_count = 0;

	workers.guids = memory.alloc<entity_guid>(count);
	if (workers.guids == nullptr) return false;

	workers.origin_entity = memory.alloc<entity_guid>(count);
	if (workers.origin_entity == nullptr) return false;

	workers.work_types = memory.alloc<WORK_TYPE>(count);
	if (workers.work_types == nullptr) return false;

	workers.work_times = memory.alloc<ui32>(count);
	if (workers.work_times == nullptr) return false;

	workers.action_data = memory.alloc<world_worker_action_data>(count);
	if (workers.action_data == nullptr) return false;

	workers.locations = memory.alloc<world_location>(count);
	if (workers.locations == nullptr) return false;

	workers.movements = memory.alloc<world_entity_movement>(count);
	if (workers.movements == nullptr) return false;

	return true;
}
