#include "game_common/match/world.h"
#include "world_sim.h"
#include "world_sim_population.h"
#include "world_sim_army.h"

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
	case ENTITY_TYPE::BUILDER:
		container = &world.entities.builders;
		break;
	case ENTITY_TYPE::BATTLE:
		container = &world.entities.battles;
		break;
	case ENTITY_TYPE::WARBAND:
		container = &world.entities.warbands;
		break;
	case ENTITY_TYPE::SIEGE:
		container = &world.entities.sieges;
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
	world.entities.settlements.local_troops[freeIndex] = start_state.local_troops;
	world.entities.settlements.troop_ceilings[freeIndex] = { 0xFFFF, 0xFFFF, 0xFFFF }; // Saturated: effective ceiling = max.

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

    // Entities start standing on their move target, so the movement system fetches the first one from a path.
    world.entities.caravans.movements[freeIndex].move_target = start_state.location;

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
	world.entities.armies.action_targets[freeIndex] = start_state.action_target;
	world.entities.armies.arrows_targets[freeIndex] = start_state.arrows_target;

    // Init default values if they weren't specified by start state.
    SET_DEFAULT_VAL(world.entities.armies.movements[freeIndex].travel_speed, 4);
    SET_DEFAULT_VAL_STRUCT(world.entities.armies.movements[freeIndex].destination, start_state.location);
    world.entities.armies.movements[freeIndex].move_target = start_state.location;

	// Not in a battle or siege, not merging, no warbands (the slot may have been used before).
	world.entities.armies.battles[freeIndex] = INVALID_ENTITY_GUID;
	world.entities.armies.sieges[freeIndex] = INVALID_ENTITY_GUID;
	world.entities.armies.merging[freeIndex] = 0;
	world.entities.armies.warbands[freeIndex].count = 0;

	return newID;
}

// Appends a warband to an army's warband list. The army must have room for it.
static void army_add_warband(match_world_state& world, ui16 army_index, ui16 warband_index)
{
	world_entity_armies::warband_list& list = world.entities.armies.warbands[army_index];
	ASSERT(list.count < ARMY_MAX_WARBANDS);

	list.indices[list.count] = warband_index;
	list.count++;
	world.entities.warbands.armies[warband_index] = world.entities.armies.guids[army_index];
}

// Takes the warband at the given slot out of an army's warband list. The slots after it move down one, keeping the joining order.
static void army_remove_warband_slot(world_entity_armies::warband_list& list, ui8 slot)
{
	for (ui8 next = slot + 1; next < list.count; next++) list.indices[next - 1] = list.indices[next];
	list.count--;
}

entity_guid world_spawn_warband(match_world_state& world, const world_entity_warbands::single& start_state, entity_guid army)
{
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	world_entity_warbands& warbands = world.entities.warbands;

	ui16 freeIndex = 0;
	while (freeIndex < warbands.max_count && warbands.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < warbands.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::WARBAND, freeIndex, world.age % 0x100); // 8-bit extra field.

	warbands.guids[freeIndex] = newID;
	warbands.active_count++;

	// Apply start state.
	warbands.owners[freeIndex] = world.entities.armies.owners[army.get_index()];
	warbands.compositions[freeIndex] = start_state.composition;
	warbands.raise_targets[freeIndex] = start_state.raise_target;
	warbands.home_settlements[freeIndex] = start_state.home_settlement;
	warbands.raising[freeIndex] = start_state.raising;

	army_add_warband(world, army.get_index(), freeIndex);

	return newID;
}

// Spawns a new battle in the world with the provided start state and returns its GUID.
entity_guid world_spawn_battle(match_world_state& world, const world_entity_battles::state& start_state)
{
	ui16 freeIndex = 0;
	while (freeIndex < world.entities.battles.max_count && world.entities.battles.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities.battles.max_count);

	entity_guid newID = entity_guid_new(ENTITY_TYPE::BATTLE, freeIndex, world.age % 0x100); // 8-bit extra field.

	world.entities.battles.guids[freeIndex] = newID;
	world.entities.battles.active_count++;
	world.entities.battles.states[freeIndex] = start_state;

	return newID;
}

entity_guid world_spawn_siege(match_world_state& world, const world_entity_sieges::state& start_state)
{
	ui16 freeIndex = 0;
	while (freeIndex < world.entities.sieges.max_count && world.entities.sieges.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities.sieges.max_count);

	entity_guid newID = entity_guid_new(ENTITY_TYPE::SIEGE, freeIndex, world.age % 0x100); // 8-bit extra field.

	world.entities.sieges.guids[freeIndex] = newID;
	world.entities.sieges.active_count++;
	world.entities.sieges.states[freeIndex] = start_state;

	return newID;
}

// Spawns a new builder in the world with the provided start state and returns its GUID.
// Returns INVALID_ENTITY_GUID if the origin is not a valid entity. Asserts if its type is not the one the build type requires.
entity_guid world_spawn_builder(match_world_state& world, const world_entity_builders::single& start_state)
{
	ASSERT(start_state.build_type < BUILD_TYPE_COUNT);
	if (!world_entity_is_valid(world, start_state.origin_entity)) return INVALID_ENTITY_GUID;
	ASSERT(start_state.origin_entity.get_type() == BUILD_TYPE_DEFINITIONS[start_state.build_type].origin_entity_type);

	ui16 freeIndex = 0;
	while (freeIndex < world.entities.builders.max_count && world.entities.builders.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities.builders.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::BUILDER, freeIndex, world.age % 0x100); // 8-bit extra field.

	world.entities.builders.guids[freeIndex] = newID;
	world.entities.builders.active_count++;

	// Apply start state.
	world.entities.builders.origin_entity[freeIndex] = start_state.origin_entity;
	world.entities.builders.build_types[freeIndex] = start_state.build_type;
	world.entities.builders.build_times[freeIndex] = start_state.build_time;
	world.entities.builders.action_data[freeIndex] = start_state.action_data;

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
	world_entity_warbands& warbands = world.entities.warbands;

	world_entity_armies::warband_list& list = armies.warbands[army.get_index()];
	for (ui8 slot = 0; slot < list.count; slot++)
	{
		warbands.guids[list.indices[slot]] = INVALID_ENTITY_GUID;
		warbands.active_count--;
	}
	list.count = 0;

	armies.guids[army.get_index()] = INVALID_ENTITY_GUID;
	armies.active_count--;
}

void world_destroy_warband(match_world_state& world, entity_guid warband)
{
	ASSERT(world_entity_is_valid(world, warband) && warband.get_type() == ENTITY_TYPE::WARBAND);
	world_entity_warbands& warbands = world.entities.warbands;
	ui16 warbandIndex = warband.get_index();

	entity_guid army = warbands.armies[warbandIndex];
	world_entity_armies::warband_list& list = world.entities.armies.warbands[army.get_index()];
	for (ui8 slot = 0; slot < list.count; slot++)
	{
		if (list.indices[slot] == warbandIndex)
		{
			army_remove_warband_slot(list, slot);
			break;
		}
	}

	warbands.guids[warbandIndex] = INVALID_ENTITY_GUID;
	warbands.active_count--;

	if (list.count == 0) world_destroy_army(world, army);
}

void world_destroy_builder(match_world_state& world, entity_guid builder)
{
	ASSERT(world_entity_is_valid(world, builder) && builder.get_type() == ENTITY_TYPE::BUILDER);
	world_entity_builders& builders = world.entities.builders;

	builders.guids[builder.get_index()] = INVALID_ENTITY_GUID;
	builders.active_count--;
}

void world_disband_army(match_world_state& world, entity_guid army, entity_guid settlement)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	ui32 men[world_entity_battles::UNIT_TYPES];
	army_total_unit_counts(world, army.get_index(), men);

	ui16 settlementIndex = settlement.get_index();
	world.entities.settlements.populations[settlementIndex] += men[0];

	// Pooled types go back to the local pool, clamped to ui16.
	settlement_troops& pool = world.entities.settlements.local_troops[settlementIndex];
	ui32 returned[3] = { (ui32)pool.men_at_arms + men[1], (ui32)pool.horsemen + men[2], (ui32)pool.knights + men[3] };
	pool.men_at_arms = (ui16)(returned[0] > 0xFFFF ? 0xFFFF : returned[0]);
	pool.horsemen = (ui16)(returned[1] > 0xFFFF ? 0xFFFF : returned[1]);
	pool.knights = (ui16)(returned[2] > 0xFFFF ? 0xFFFF : returned[2]);
	world_destroy_army(world, army);
}

void world_destroy_battle(match_world_state& world, entity_guid battle)
{
	ASSERT(world_entity_is_valid(world, battle) && battle.get_type() == ENTITY_TYPE::BATTLE);
	world_entity_battles& battles = world.entities.battles;

	battles.guids[battle.get_index()] = INVALID_ENTITY_GUID;
	battles.active_count--;
}

void world_destroy_siege(match_world_state& world, entity_guid siege)
{
	ASSERT(world_entity_is_valid(world, siege) && siege.get_type() == ENTITY_TYPE::SIEGE);
	world_entity_sieges& sieges = world.entities.sieges;
	world_entity_armies& armies = world.entities.armies;

	// Its armies are free to move again.
	const world_entity_sieges::state& state = sieges.states[siege.get_index()];
	for (ui8 slot = 0; slot < state.army_count; slot++)
	{
		entity_guid army = state.armies[slot];
		if (world_entity_is_valid(world, army) && armies.sieges[army.get_index()] == siege) armies.sieges[army.get_index()] = INVALID_ENTITY_GUID;
	}

	sieges.guids[siege.get_index()] = INVALID_ENTITY_GUID;
	sieges.active_count--;
}

entity_guid world_spawn_settlement_defenders(match_world_state& world, entity_guid settlement)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	world_entity_settlements& settlements = world.entities.settlements;
	ui16 settlementIndex = settlement.get_index();

	ui32& population = settlements.populations[settlementIndex];
	settlement_troops& pool = settlements.local_troops[settlementIndex];
	world_entity_warbands::comp men = settlement_defender_men(population, pool);

	// The men are taken out of the settlement (not claimed): world_disband_army gives them back.
	population -= men.levies < population ? men.levies : population;
	pool = {};

	world_entity_armies::single defenders = {};
	defenders.owner = settlements.owners[settlementIndex];
	defenders.location = settlements.locations[settlementIndex];
	entity_guid defenderArmy = world_spawn_army(world, defenders);

	// One warband, with no home settlement.
	world_entity_warbands::single defenderWarband = {};
	defenderWarband.composition = men;
	defenderWarband.raise_target = men; // Initial counts, as "original counts" for the client. Not raising.
	world_spawn_warband(world, defenderWarband, defenderArmy);

	return defenderArmy;
}

entity_guid world_settlement_siege(const match_world_state& world, entity_guid settlement)
{
	const world_entity_sieges& sieges = world.entities.sieges;
	for (ui32 siegeIndex = 0; siegeIndex < sieges.max_count; siegeIndex++)
	{
		if (sieges.guids[siegeIndex].is_valid() && sieges.states[siegeIndex].settlement == settlement) return sieges.guids[siegeIndex];
	}
	return INVALID_ENTITY_GUID;
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

void world_settlement_raised(const match_world_state& world, entity_guid settlement, ui32& out_warbands, ui32& out_men)
{
	const world_entity_warbands& warbands = world.entities.warbands;
	match_player_id owner = world.entities.settlements.owners[settlement.get_index()];

	out_warbands = 0;
	out_men = 0;
	for (ui32 warbandIndex = 0; warbandIndex < warbands.max_count; warbandIndex++)
	{
		if (!warbands.guids[warbandIndex].is_valid() || !(warbands.home_settlements[warbandIndex] == settlement) || warbands.owners[warbandIndex] != owner) continue;

		out_warbands++;
		out_men += (ui32)army_total_manpower(warbands.compositions[warbandIndex]);
	}
}

void world_warband_remove_men_from_home(match_world_state& world, ui16 warband_index, const world_entity_warbands::comp& men)
{
	entity_guid home = world.entities.warbands.home_settlements[warband_index];
	if (!world_entity_is_valid(world, home) || world.entities.settlements.owners[home.get_index()] != world.entities.warbands.owners[warband_index]) return;

	ui16 homeIndex = home.get_index();
	ui32& population = world.entities.settlements.populations[homeIndex];
	population = population > (ui32)men.levies + 1 ? population - men.levies : 1;

	settlement_troops& pool = world.entities.settlements.local_troops[homeIndex];
	pool.men_at_arms = pool.men_at_arms > men.men_at_arms ? pool.men_at_arms - men.men_at_arms : 0;
	pool.horsemen = pool.horsemen > men.horsemen ? pool.horsemen - men.horsemen : 0;
	pool.knights = pool.knights > men.knights ? pool.knights - men.knights : 0;
}

bool world_army_merge(match_world_state& world, entity_guid army, entity_guid into_army)
{
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	ASSERT(world_entity_is_valid(world, into_army) && into_army.get_type() == ENTITY_TYPE::ARMY && !(army == into_army));
	world_entity_armies& armies = world.entities.armies;
	ui16 intoIndex = into_army.get_index();

	world_entity_armies::warband_list& from = armies.warbands[army.get_index()];
	if ((ui32)from.count + armies.warbands[intoIndex].count > ARMY_MAX_WARBANDS) return false;

	for (ui8 slot = 0; slot < from.count; slot++) army_add_warband(world, intoIndex, from.indices[slot]);
	from.count = 0; // Moved: destroying the emptied army must not destroy them.
	world_destroy_army(world, army);
	return true;
}

entity_guid world_army_detach(match_world_state& world, entity_guid army, ui64 slots)
{
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	world_entity_armies& armies = world.entities.armies;
	ui16 armyIndex = army.get_index();

	// First walkable tile around the army, in neighbour order. Its own tile if there is none.
	const world_terrain& terrain = world.terrain;
	const path_params landParams = { PATH_TERRAIN_LAND }; // Other members zero-initialized.
	world_location location = armies.locations[armyIndex];
	world_location spot = location;
	for (const vec2<i8>& offset : NEIGHBOUR_OFFSETS_ALL)
	{
		i32 x = (i32)location.x + offset.x;
		i32 y = (i32)location.y + offset.y;
		if (x < 0 || y < 0 || x >= (i32)terrain.size_tiles.x || y >= (i32)terrain.size_tiles.y) continue;

		world_location candidate = { (ui16)x, (ui16)y };
		ui16 regionIndex, tileIndex;
		world_get_tile(terrain, candidate, regionIndex, tileIndex);
		if (world_tile_move_cost(terrain, regionIndex, tileIndex, landParams) == 0) continue;

		spot = candidate;
		break;
	}

	world_entity_armies::single start = {};
	start.owner = armies.owners[armyIndex];
	start.location = spot;
	start.movement.travel_speed = armies.movements[armyIndex].travel_speed;
	entity_guid newArmy = world_spawn_army(world, start);

	// Selected warbands in slot order, then taken out from the last slot down so the slots still to visit keep their position.
	world_entity_armies::warband_list& list = armies.warbands[armyIndex];
	ASSERT(slots != 0 && list.count <= ARMY_MAX_WARBANDS);
	ui16 moved[ARMY_MAX_WARBANDS];
	ui8 movedCount = 0;
	for (ui8 slot = 0; slot < list.count; slot++)
	{
		if (slots & ((ui64)1 << slot)) moved[movedCount++] = list.indices[slot];
	}
	for (i32 slot = (i32)list.count - 1; slot >= 0; slot--)
	{
		if (slots & ((ui64)1 << slot)) army_remove_warband_slot(list, (ui8)slot);
	}
	ASSERT(list.count > 0);

	for (ui8 movedIndex = 0; movedIndex < movedCount; movedIndex++) army_add_warband(world, newArmy.get_index(), moved[movedIndex]);
	return newArmy;
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
	case ENTITY_TYPE::BUILDER:
		// Builders have no location of their own for now: they are where their origin is.
		return world_get_entity_location(world, world.entities.builders.origin_entity[entity.get_index()]);
	case ENTITY_TYPE::BATTLE:
		return world.entities.battles.states[entity.get_index()].location;
	case ENTITY_TYPE::WARBAND:
		// Warbands stand where their army is.
		return world.entities.armies.locations[world.entities.warbands.armies[entity.get_index()].get_index()];
	case ENTITY_TYPE::SIEGE:
		return world.entities.sieges.states[entity.get_index()].location;
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

	ui32 raisedArmies, raisedMen;
	world_settlement_raised(world, entity, raisedArmies, raisedMen);

	out_state = {
		.owner = settlements.owners[i],
		.location = settlements.locations[i],
		.population = settlements.populations[i],
		.local_wealth = settlements.local_wealths[i],
        .land_output = settlements.land_outputs[i],
		.influence = settlements.influences[i],
		.local_troops = settlements.local_troops[i],
		.troop_ceilings = settlements.troop_ceilings[i],
		.troop_max = settlement_troop_max(settlements.populations[i]),
		.troop_effective_ceilings = settlement_troop_effective_ceilings(settlements.populations[i], settlements.troop_ceilings[i]),
		.raised_armies = (ui8)raisedArmies,
		.max_raised_armies = SETTLEMENT_MAX_RAISED_ARMIES,
		.raised_men = raisedMen,
		.raised_percent = settlement_raised_percent(raisedMen, settlements.populations[i]),
		.siege = world_settlement_siege(world, entity),
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
	const world_entity_warbands& warbands = world.entities.warbands;
	ui16 i = entity.get_index();

	entity_guid target = armies.action_targets[i];
	bool followsOwnArmy = target.get_type() == ENTITY_TYPE::ARMY && world_entity_is_valid(world, target) && armies.owners[target.get_index()] == armies.owners[i];
	bool raising = army_is_raising(world, i);

	ui8 state = ARMY_STATE_IDLE;
	if (armies.battles[i].is_valid()) state = ARMY_STATE_IN_BATTLE;
	else if (armies.sieges[i].is_valid()) state = ARMY_STATE_BESIEGING;
	else if (raising) state = ARMY_STATE_RAISING;
	else if (followsOwnArmy) state = armies.merging[i] ? ARMY_STATE_MERGING : ARMY_STATE_FOLLOWING;
	else if (target.is_valid()) state = ARMY_STATE_ATTACKING;
	else if (!(armies.locations[i] == armies.movements[i].destination)) state = ARMY_STATE_MOVING;

	ui32 men[world_entity_battles::UNIT_TYPES];
	army_total_unit_counts(world, i, men);

	out_state = {
		.owner = armies.owners[i],
		.location = armies.locations[i],
		.movement = armies.movements[i],
		.action_target = target,
		.arrows_target = armies.arrows_targets[i],
		.merging = armies.merging[i],
		.battle = armies.battles[i],
		.siege = armies.sieges[i],
		.state = state,
		.raising = (ui8)(raising ? 1 : 0),
		.followed_army = followsOwnArmy ? target : INVALID_ENTITY_GUID,
		.composition = { men[0], men[1], men[2], men[3] },
		.warband_count = armies.warbands[i].count,
	};

	for (ui8 slot = 0; slot < armies.warbands[i].count; slot++) out_state.warbands[slot] = warbands.guids[armies.warbands[i].indices[slot]];
	return true;
}

bool query_entity_state_warband(const match_world_state& world, entity_guid entity, world_entity_warbands::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::WARBAND || !world_entity_is_valid(world, entity)) return false;

	const world_entity_warbands& warbands = world.entities.warbands;
	ui16 i = entity.get_index();

	out_state = {
		.owner = warbands.owners[i],
		.composition = warbands.compositions[i],
		.raise_target = warbands.raise_targets[i],
		.home_settlement = warbands.home_settlements[i],
		.raising = warbands.raising[i],
		.army = warbands.armies[i],
	};
	return true;
}

bool query_entity_state_builder(const match_world_state& world, entity_guid entity, world_entity_builders::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::BUILDER || !world_entity_is_valid(world, entity)) return false;

	const world_entity_builders& builders = world.entities.builders;
	ui16 i = entity.get_index();

	out_state = {
		.origin_entity = builders.origin_entity[i],
		.build_type = builders.build_types[i],
		.build_time = builders.build_times[i],
		.action_data = builders.action_data[i],
	};
	return true;
}

bool query_entity_state_battle(const match_world_state& world, entity_guid entity, world_entity_battles::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::BATTLE || !world_entity_is_valid(world, entity)) return false;

	out_state = world.entities.battles.states[entity.get_index()];
	return true;
}

bool query_entity_state_siege(const match_world_state& world, entity_guid entity, world_entity_sieges::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::SIEGE || !world_entity_is_valid(world, entity)) return false;

	const world_entity_sieges::state& siege = world.entities.sieges.states[entity.get_index()];
	out_state = {};
	out_state.siege = siege;
	if (siege.growth_ticks <= SIEGE_STARVATION_GRACE_GROWTH_TICKS) out_state.starvation_in = (ui16)(SIEGE_STARVATION_GRACE_GROWTH_TICKS + 1 - siege.growth_ticks);
	if (siege.growth_ticks < SIEGE_SURRENDER_GROWTH_TICKS) out_state.surrender_in = (ui16)(SIEGE_SURRENDER_GROWTH_TICKS - siege.growth_ticks);

	// Besiegers: every army still in the siege (an army destroyed this tick may still be listed until the siege system runs).
	for (ui8 slot = 0; slot < siege.army_count; slot++)
	{
		entity_guid army = siege.armies[slot];
		if (!world_entity_is_valid(world, army) || !(world.entities.armies.sieges[army.get_index()] == entity)) continue;

		ui32 men[world_entity_battles::UNIT_TYPES];
		army_total_unit_counts(world, army.get_index(), men);
		for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++) out_state.besieger_men[unitType] += men[unitType];
	}

	// Defenders: what world_spawn_settlement_defenders would field now.
	if (world_entity_is_valid(world, siege.settlement))
	{
		ui16 settlementIndex = siege.settlement.get_index();
		world_entity_warbands::comp defenders = settlement_defender_men(world.entities.settlements.populations[settlementIndex], world.entities.settlements.local_troops[settlementIndex]);
		army_unit_counts(defenders, out_state.defender_men);
	}
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

bool world_is_valid_settlement_location(const match_world_state& world, world_location loc, match_player_id player)
{
	const world_terrain& terrain = world.terrain;
	if (loc.x >= terrain.size_tiles.x || loc.y >= terrain.size_tiles.y) return false;

	ui16 regionIndex, tileIndex;
	world_get_tile(terrain, loc, regionIndex, tileIndex);

	TERRAIN_TILE_TYPE type = terrain.regions[regionIndex].terrain_types[tileIndex];
	if (type != TERRAIN_TILE_TYPE::LAND_PLAINS && type != TERRAIN_TILE_TYPE::LAND_HILLS) return false;

	const world_entity_settlements& settlements = world.entities.settlements;

	// Tile owner is a settlement slot index.
	ui16 tileOwner = terrain.regions[regionIndex].owners[tileIndex];
	if (tileOwner != world_terrain::UNOWNED_TILE && settlements.owners[tileOwner] != player) return false;

	for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid()) continue;

		i32 dx = (i32)settlements.locations[settlementIndex].x - (i32)loc.x;
		i32 dy = (i32)settlements.locations[settlementIndex].y - (i32)loc.y;
		if (dx * dx + dy * dy < (i32)(SETTLEMENT_MIN_DISTANCE * SETTLEMENT_MIN_DISTANCE)) return false;
	}

	return true;
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

	settlements.local_troops = memory.alloc<settlement_troops>(count);
	if (settlements.local_troops == nullptr) return false;

	settlements.troop_ceilings = memory.alloc<settlement_troops>(count);
	if (settlements.troop_ceilings == nullptr) return false;

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

bool world_alloc_builders(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_builders& builders = world.entities.builders;

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

	armies.action_targets = memory.alloc<entity_guid>(count);
	if (armies.action_targets == nullptr) return false;

	armies.arrows_targets = memory.alloc<entity_guid>(count);
	if (armies.arrows_targets == nullptr) return false;

	armies.merging = memory.alloc<ui8>(count);
	if (armies.merging == nullptr) return false;

	armies.battles = memory.alloc<entity_guid>(count);
	if (armies.battles == nullptr) return false;

	armies.battle_sides = memory.alloc<ui8>(count);
	if (armies.battle_sides == nullptr) return false;

	armies.sieges = memory.alloc<entity_guid>(count);
	if (armies.sieges == nullptr) return false;

	armies.warbands = memory.alloc<world_entity_armies::warband_list>(count);
	if (armies.warbands == nullptr) return false;

	return true;
}

bool world_alloc_warbands(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_warbands& warbands = world.entities.warbands;

	warbands.max_count = count;

	warbands.guids = memory.alloc<entity_guid>(count);
	if (warbands.guids == nullptr) return false;

	warbands.owners = memory.alloc<match_player_id>(count);
	if (warbands.owners == nullptr) return false;

	warbands.compositions = memory.alloc<world_entity_warbands::comp>(count);
	if (warbands.compositions == nullptr) return false;

	warbands.raise_targets = memory.alloc<world_entity_warbands::comp>(count);
	if (warbands.raise_targets == nullptr) return false;

	warbands.home_settlements = memory.alloc<entity_guid>(count);
	if (warbands.home_settlements == nullptr) return false;

	warbands.raising = memory.alloc<ui8>(count);
	if (warbands.raising == nullptr) return false;

	warbands.armies = memory.alloc<entity_guid>(count);
	if (warbands.armies == nullptr) return false;

	return true;
}

bool world_alloc_battles(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_battles& battles = world.entities.battles;

	battles.max_count = count;

	battles.guids = memory.alloc<entity_guid>(count);
	if (battles.guids == nullptr) return false;

	battles.states = memory.alloc<world_entity_battles::state>(count);
	if (battles.states == nullptr) return false;

	return true;
}

bool world_alloc_sieges(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_sieges& sieges = world.entities.sieges;

	sieges.max_count = count;

	sieges.guids = memory.alloc<entity_guid>(count);
	if (sieges.guids == nullptr) return false;

	sieges.states = memory.alloc<world_entity_sieges::state>(count);
	if (sieges.states == nullptr) return false;

	return true;
}


