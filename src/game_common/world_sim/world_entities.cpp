// Entity functions that span several entity types. Per-entity lifecycle and queries are in each entity's folder.

#include "game_common/match/world/world_terrain.h"
#include "game_common/match/world/world_entities.h"
#include "world_sim.h"

bool world_entity_is_valid(const match_world_state& world, entity_guid entity)
{
	if (!entity.is_valid()) return false;

	const world_entity_container* container;

	switch (entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
		container = &world.entities->settlements;
		break;
	case ENTITY_TYPE::ARMY:
		container = &world.entities->armies;
		break;
	case ENTITY_TYPE::BUILDER:
		container = &world.entities->builders;
		break;
	case ENTITY_TYPE::BATTLE:
		container = &world.entities->battles;
		break;
	default:
		return false;
	}

	// The stored GUID is zero for inactive slots, and differs in its extra bits if the slot was reused by a newer entity.
	ui16 index = entity.get_index();
	return index < container->max_count && container->guids[index] == entity;
}

world_location world_entity_get_location(const match_world_state& world, entity_guid entity)
{
	ENTITY_TYPE type = entity.get_type();

	switch (type)
	{
	case ENTITY_TYPE::SETTLEMENT:
		return world.entities->settlements.locations[entity.get_index()];
	case ENTITY_TYPE::ARMY:
		return world.entities->armies.locations[entity.get_index()];
	case ENTITY_TYPE::BUILDER:
		// Builders have no location of their own for now: they are where their origin is.
		return world_entity_get_location(world, world.entities->builders.origin_entity[entity.get_index()]);
	case ENTITY_TYPE::BATTLE:
		return world.entities->battles.states[entity.get_index()].location;
	default:
		// .. TODO(Marc): Need to be able to report error on the match / world level somehow. Provide a function pointer ?
		return INVALID_WORLD_LOCATION;
	}
}
