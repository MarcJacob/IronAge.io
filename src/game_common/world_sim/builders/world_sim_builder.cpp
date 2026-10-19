#include "world_sim_builder.h"
#include "../world_sim.h"

entity_guid world_spawn_builder(match_world_state& world, const world_entity_builders::single& start_state)
{
	ASSERT(start_state.build_type < BUILD_TYPE_COUNT);
	if (!world_entity_is_valid(world, start_state.origin_entity)) return INVALID_ENTITY_GUID;
	ASSERT(start_state.origin_entity.get_type() == BUILD_TYPE_DEFINITIONS[start_state.build_type].origin_entity_type);

	ui16 freeIndex = 0;
	while (freeIndex < world.entities->builders.max_count && world.entities->builders.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities->builders.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::BUILDER, freeIndex, world.age % 0x100); // 8-bit extra field.

	world.entities->builders.guids[freeIndex] = newID;
	world.entities->builders.active_count++;

	// Apply start state.
	world.entities->builders.origin_entity[freeIndex] = start_state.origin_entity;
	world.entities->builders.build_types[freeIndex] = start_state.build_type;
	world.entities->builders.build_times[freeIndex] = start_state.build_time;
	world.entities->builders.action_data[freeIndex] = start_state.action_data;

	return newID;
}

void world_destroy_builder(match_world_state& world, entity_guid builder)
{
	ASSERT(world_entity_is_valid(world, builder) && builder.get_type() == ENTITY_TYPE::BUILDER);
	world_entity_builders& builders = world.entities->builders;

	builders.guids[builder.get_index()] = INVALID_ENTITY_GUID;
	builders.active_count--;
}

bool world_entity_query_builder(const match_world_state& world, entity_guid entity, world_entity_builders::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::BUILDER || !world_entity_is_valid(world, entity)) return false;

	const world_entity_builders& builders = world.entities->builders;
	ui16 i = entity.get_index();

	out_state = {
		.origin_entity = builders.origin_entity[i],
		.build_type = builders.build_types[i],
		.build_time = builders.build_times[i],
		.action_data = builders.action_data[i],
	};
	return true;
}

bool world_alloc_builders(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_builders& builders = world.entities->builders;

	builders.max_count = count;
	builders.active_count = 0;

	builders.guids = memory.alloc<entity_guid>(count);
	if (builders.guids == nullptr) return false;

	builders.origin_entity = memory.alloc<entity_guid>(count);
	if (builders.origin_entity == nullptr) return false;

	builders.build_types = memory.alloc<ui16>(count);
	if (builders.build_types == nullptr) return false;

	builders.build_times = memory.alloc<ui32>(count);
	if (builders.build_times == nullptr) return false;

	builders.action_data = memory.alloc<world_build_action_data>(count);
	if (builders.action_data == nullptr) return false;

	return true;
}
