#include "game_common/match/world.h"
#include "world_sim.h"

// Used to initialize default values on entity property fields if they are left at 0.
#define SET_DEFAULT_VAL(value, default) \
    (value = (value != 0 ? (value) : ((__typeof(value))default)))

#define SET_DEFAULT_VAL_STRUCT(value, default) \
    (value = (STRUCT_IS_INITIALIZED(value) ? (value) : ((__typeof(value))default)))

bool world_entity_is_valid(const match_world_state& world, entity_guid entity)
{
	if (!entity.is_valid()) return false;

	const world_entity_container* container;

	switch (entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
		container = &world.entities.settlements;
		break;
	case ENTITY_TYPE::CARAVAN:
		container = &world.entities.caravans;
		break;
	case ENTITY_TYPE::ARMY:
		container = &world.entities.armies;
		break;
	default:
		return false;
	}

	// The stored GUID is zero for inactive slots, and differs in its extra bits if the slot was reused by a newer entity.
	ui16 index = entity.get_index();
	return index < container->max_count && container->guids[index] == entity;
}

// Spawns a new settlement in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_settlement(match_world_state& world, const world_entity_settlements::single& start_state)
{
	ui16 freeIndex = 0;
	while (freeIndex < world.entities.settlements.max_count && world.entities.settlements.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities.settlements.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::SETTLEMENT, freeIndex, world.age % 0x1000); // 12-bit extra field.

	world.entities.settlements.guids[freeIndex] = newID;
	world.entities.settlements.active_count++;

	// Apply start state.
	world.entities.settlements.owners[freeIndex] = start_state.owner;
	world.entities.settlements.locations[freeIndex] = start_state.location;
	world.entities.settlements.populations[freeIndex] = start_state.population;
	world.entities.settlements.local_wealths[freeIndex] = start_state.local_wealth;
	world.entities.settlements.tiers[freeIndex] = start_state.tier;

    // Init default values if they weren't specified by start state.
    SET_DEFAULT_VAL(world.entities.settlements.populations[freeIndex], 100);
    SET_DEFAULT_VAL_STRUCT(world.entities.settlements.influences[freeIndex], (world_entity_settlements::land_influence
                {
                .bounds_min = world.entities.settlements.locations[freeIndex],
                .bounds_max = world.entities.settlements.locations[freeIndex],
                .pressure = 0,
                }));

    // Claim own tile and set it as border to begin influence field.
    ui16 regionIndex, tileIndex;
    world_get_tile(world.terrain, start_state.location, regionIndex, tileIndex);

    world_influence_claim_tile(world, regionIndex, tileIndex, freeIndex);

	return newID;
}

// Spawns a new caravan in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_caravan(match_world_state& world, const world_entity_caravans::single& start_state)
{
	ui16 freeIndex = 0;
	while (freeIndex < world.entities.caravans.max_count && world.entities.caravans.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities.caravans.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::CARAVAN, freeIndex, world.age % 0x100); // 8-bit extra field.

	world.entities.caravans.guids[freeIndex] = newID;
	world.entities.caravans.active_count++;

	// Apply start state.
	world.entities.caravans.locations[freeIndex] = start_state.location;
	world.entities.caravans.movements[freeIndex] = start_state.movement;
	world.entities.caravans.origin_settlements[freeIndex] = start_state.origin_settlement;
	world.entities.caravans.dest_settlements[freeIndex] = start_state.dest_settlement;

    // Initialize default values if they were missing from the provided start state.
    SET_DEFAULT_VAL(world.entities.caravans.movements[freeIndex].travel_speed, 10);
    // ...

	return newID;
}

// Spawns a new army in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_army(match_world_state& world, const world_entity_armies::single& start_state)
{
	ui16 freeIndex = 0;
	while (freeIndex < world.entities.armies.max_count && world.entities.armies.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities.armies.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::ARMY, freeIndex, world.age % 0x100); // 8-bit extra field.

	world.entities.armies.guids[freeIndex] = newID;
	world.entities.armies.active_count++;

	// Apply start state.
	world.entities.armies.owners[freeIndex] = start_state.owner;
	world.entities.armies.locations[freeIndex] = start_state.location;
	world.entities.armies.movements[freeIndex] = start_state.movement;
	world.entities.armies.compositions[freeIndex] = start_state.composition;
	world.entities.armies.action_targets[freeIndex] = start_state.action_target;
	world.entities.armies.arrows_targets[freeIndex] = start_state.arrows_target;

    // Init default values if they weren't specified by start state.
    SET_DEFAULT_VAL(world.entities.armies.movements[freeIndex].travel_speed, 12);
    SET_DEFAULT_VAL_STRUCT(world.entities.armies.movements[freeIndex].move_target, start_state.location);

	return newID;
}


void world_destroy_settlement(match_world_state& world, entity_guid settlement)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	world_entity_settlements& settlements = world.entities.settlements;

	settlements.guids[settlement.get_index()] = INVALID_ENTITY_GUID;
	settlements.active_count--;
}

void world_destroy_caravan(match_world_state& world, entity_guid caravan)
{
    ASSERT(world_entity_is_valid(world, caravan));
    world_entity_caravans& caravans = world.entities.caravans;

    caravans.guids[caravan.get_index()] = INVALID_ENTITY_GUID;
    caravans.active_count--;
}

void world_destroy_army(match_world_state& world, entity_guid army)
{
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	world_entity_armies& armies = world.entities.armies;

	armies.guids[army.get_index()] = INVALID_ENTITY_GUID;
	armies.active_count--;
}

void world_settlement_decrease_population(match_world_state& world, entity_guid settlement, ui32 amount)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	ui32& population = world.entities.settlements.populations[settlement.get_index()];

	population = amount < population ? population - amount : 0;
	if (population == 0)
	{
		world_destroy_settlement(world, settlement);
	}
}

world_location world_get_entity_location(const match_world_state& world, entity_guid entity)
{
	ENTITY_TYPE type = entity.get_type();

	switch (type)
	{
	case ENTITY_TYPE::SETTLEMENT:
		return world.entities.settlements.locations[entity.get_index()];
	case ENTITY_TYPE::CARAVAN:
		return world.entities.caravans.locations[entity.get_index()];
	case ENTITY_TYPE::ARMY:
		return world.entities.armies.locations[entity.get_index()];
	default:
		// .. TODO(Marc): Need to be able to report error on the match / world level somehow. Provide a function pointer ?
		return INVALID_WORLD_LOCATION;
	}
}

bool query_entity_state_settlement(const match_world_state& world, entity_guid entity, world_entity_settlements::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::SETTLEMENT || !world_entity_is_valid(world, entity)) return false;

	const world_entity_settlements& settlements = world.entities.settlements;
	ui16 i = entity.get_index();

	out_state = {
		.owner = settlements.owners[i],
		.location = settlements.locations[i],
		.population = settlements.populations[i],
		.local_wealth = settlements.local_wealths[i],
        .land_output = settlements.land_outputs[i],
		.influence = settlements.influences[i],
		.tier = settlements.tiers[i],
		.trade_attaction_index = settlements.trade_attraction_indices[i],
	};
	return true;
}

bool query_entity_state_caravan(const match_world_state& world, entity_guid entity, world_entity_caravans::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::CARAVAN || !world_entity_is_valid(world, entity)) return false;

	const world_entity_caravans& caravans = world.entities.caravans;
	ui16 i = entity.get_index();

	out_state = {
		.location = caravans.locations[i],
		.movement = caravans.movements[i],
		.origin_settlement = caravans.origin_settlements[i],
		.dest_settlement = caravans.dest_settlements[i],
	};
	return true;
}

bool query_entity_state_army(const match_world_state& world, entity_guid entity, world_entity_armies::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::ARMY || !world_entity_is_valid(world, entity)) return false;

	const world_entity_armies& armies = world.entities.armies;
	ui16 i = entity.get_index();

	out_state = {
		.owner = armies.owners[i],
		.location = armies.locations[i],
		.movement = armies.movements[i],
		.composition = armies.compositions[i],
		.action_target = armies.action_targets[i],
		.arrows_target = armies.arrows_targets[i],
	};
	return true;
}

bool world_caravan_is_owned_by(const match_world_state& world, entity_guid caravan, match_player_id player)
{
	world_entity_caravans::single state;
	if (!query_entity_state_caravan(world, caravan, state)) return false;

	world_entity_settlements::single settlement;
	if (query_entity_state_settlement(world, state.origin_settlement, settlement) && settlement.owner == player) return true;
	return query_entity_state_settlement(world, state.dest_settlement, settlement) && settlement.owner == player;
}

bool world_alloc_settlements(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_settlements& settlements = world.entities.settlements;

	settlements.max_count = count;
	settlements.active_count = 0;

	settlements.guids = memory.alloc<entity_guid>(count);
	if (settlements.guids == nullptr) return false;

	settlements.owners = memory.alloc<match_player_id>(count);
	if (settlements.owners == nullptr) return false;

	settlements.locations = memory.alloc<world_location>(count);
	if (settlements.locations == nullptr) return false;

	settlements.populations = memory.alloc<ui32>(count);
	if (settlements.populations == nullptr) return false;

	settlements.local_wealths = memory.alloc<ui32>(count);
	if (settlements.local_wealths == nullptr) return false;

    settlements.land_sizes = memory.alloc<ui32>(count);
    if (settlements.land_sizes == nullptr) return false;

    settlements.land_outputs = memory.alloc<ui32>(count);
    if (settlements.land_outputs == nullptr) return false;

	settlements.tiers = memory.alloc<ui8>(count);
	if (settlements.tiers == nullptr) return false;

	settlements.trade_attraction_indices = memory.alloc<ui8>(count);
	if (settlements.trade_attraction_indices == nullptr) return false;

	settlements.influences = memory.alloc<world_entity_settlements::land_influence>(count);
	if (settlements.influences == nullptr) return false;

	return true;
}

bool world_alloc_caravans(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_caravans& caravans = world.entities.caravans;

	caravans.max_count = count;

	caravans.guids = memory.alloc<entity_guid>(count);
	if (caravans.guids == nullptr) return false;

	caravans.locations = memory.alloc<world_location>(count);
	if (caravans.locations == nullptr) return false;

	caravans.movements = memory.alloc<world_entity_movement>(count);
	if (caravans.movements == nullptr) return false;

	caravans.origin_settlements = memory.alloc<entity_guid>(count);
	if (caravans.origin_settlements == nullptr) return false;

	caravans.dest_settlements = memory.alloc<entity_guid>(count);
	if (caravans.dest_settlements == nullptr) return false;

	return true;
}

bool world_alloc_armies(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_armies& armies = world.entities.armies;

	armies.max_count = count;

	armies.guids = memory.alloc<entity_guid>(count);
	if (armies.guids == nullptr) return false;

	armies.owners = memory.alloc<match_player_id>(count);
	if (armies.owners == nullptr) return false;

	armies.locations = memory.alloc<world_location>(count);
	if (armies.locations == nullptr) return false;

	armies.movements = memory.alloc<world_entity_movement>(count);
	if (armies.movements == nullptr) return false;

	armies.compositions = memory.alloc<world_entity_armies::comp>(count);
	if (armies.compositions == nullptr) return false;

	armies.action_targets = memory.alloc<entity_guid>(count);
	if (armies.action_targets == nullptr) return false;

	armies.arrows_targets = memory.alloc<entity_guid>(count);
	if (armies.arrows_targets == nullptr) return false;

	return true;
}


