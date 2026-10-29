#include "world_sim_entity_worker.h"
#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world_terrain.h"

entity_guid world_spawn_worker(match_world_state& world, const world_entity_workers::single& start_state)
{
	ASSERT(start_state.work_type < WORK_TYPE::WORK_TYPE_COUNT);
	if (!world_entity_is_valid(world, start_state.origin_entity)) return INVALID_ENTITY_GUID;

    // @TODO(Marc): Similarly to input command, have a validation function for each work type.

	world_entity_workers& workers = world.entities->workers;
	ui16 freeIndex = 0;
	while (freeIndex < workers.max_count && workers.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < workers.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::WORKER, freeIndex, world.age % 0x100); // 8-bit extra field.

	workers.guids[freeIndex] = newID;
	workers.active_count++;

	// Apply start state.
	world_entity_workers::view newWorker { workers, freeIndex };
	newWorker.copy_from(start_state);

	// Every worker starts at its origin. Moving work types take it from there (start_state.position / movement are not read).
	world_location start = world_entity_get_location(world, start_state.origin_entity);
	workers.location[freeIndex] = start;
	world_entity_movement& movement = workers.movement[freeIndex];
	movement = {};
	movement.travel_speed = (ui16)GAME_PARAMETERS.worker_travel_speed;
	movement.destination = start;
	movement.path_handle = INVALID_PATH_HANDLE;

	return newID;
}

void world_destroy_worker(match_world_state& world, entity_guid worker)
{
	ASSERT(world_entity_is_valid(world, worker) && worker.get_type() == ENTITY_TYPE::WORKER);
	world_entity_workers& workers = world.entities->workers;

	ui16 index = worker.get_index();
	world_entity_worker_view destroyedWorker { workers, index };
	destroyedWorker.copy_from(world_entity_worker {});
	workers.guids[index] = INVALID_ENTITY_GUID;
	workers.active_count--;
}

bool world_entity_query_worker(const match_world_state& world, entity_guid entity, world_entity_workers::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::WORKER || !world_entity_is_valid(world, entity)) return false;

	const world_entity_workers& workers = world.entities->workers;
	ui16 index = entity.get_index();

	out_state.copy_from(workers, index);
	out_state.owner = world_entity_get_owner(world, out_state.origin_entity);
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

	workers.work_type = memory.alloc<WORK_TYPE>(count);
	if (workers.work_type == nullptr) return false;

	workers.work_time = memory.alloc<ui32>(count);
	if (workers.work_time == nullptr) return false;

    workers.work_progress_ratio = memory.alloc<ui8>(count);
    if (workers.work_progress_ratio == nullptr) return false;

	workers.daily_cost = memory.alloc<ui32>(count);
	if (workers.daily_cost == nullptr) return false;

	workers.action_data = memory.alloc<world_worker_action_data>(count);
	if (workers.action_data == nullptr) return false;

	workers.location = memory.alloc<world_location>(count);
	if (workers.location == nullptr) return false;

	workers.movement = memory.alloc<world_entity_movement>(count);
	if (workers.movement == nullptr) return false;

	return true;
}
