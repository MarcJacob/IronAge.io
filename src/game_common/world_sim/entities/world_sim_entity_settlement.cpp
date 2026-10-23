#include "world_sim_entity_settlement.h"
#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world.h"
#include "game_common/match/world/world_terrain.h"

entity_guid world_spawn_settlement(match_world_state& world, const world_entity_settlements::single& start_state)
{
	ui16 freeIndex = 0;
	while (freeIndex < world.entities->settlements.max_count && world.entities->settlements.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities->settlements.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::SETTLEMENT, freeIndex, world.age % 0x1000); // 12-bit extra field.

	world.entities->settlements.guids[freeIndex] = newID;
	world.entities->settlements.active_count++;

	// Apply start state.
	world.entities->settlements.owners[freeIndex] = start_state.owner;

	world.entities->settlements.locations[freeIndex] = start_state.location;
    world.entities->settlements.founding_ticks[freeIndex] = world.age;

    world.entities->settlements.base_land_outputs[freeIndex] = start_state.base_land_output;
	world.entities->settlements.pending_incomes[freeIndex] = start_state.pending_income;
	world.entities->settlements.tax_rates[freeIndex] = start_state.tax_rate;
	world.entities->settlements.economies[freeIndex] = start_state.economy;
    world.entities->settlements.territories[freeIndex] = start_state.territory;
	world.entities->settlements.troop_ceilings[freeIndex] = start_state.troop_ceilings;

	// Slots are reused: no buildings to start with.
	for (ui8 buildingType = 0; buildingType < SETTLEMENT_BUILDING_TYPE_COUNT; buildingType++)
	{
		world.entities->settlements.buildings[freeIndex].building_counts[buildingType] = 0;
	}

	// Slots are reused: no caravans and no caravan capacity to start with (buildings add to it).
	world.entities->settlements.trade[freeIndex] = {};

	settlement_population& population = world.entities->settlements.populations[freeIndex];
    population = start_state.population;

    // Init default values if they weren't specified by start state.
    SET_DEFAULT_VAL(population.peasants, 100);
	population.population_total = population.at_home() + population.raised.levies + population.raised.men_at_arms + population.raised.horsemen + population.raised.knights;
    SET_DEFAULT_VAL_STRUCT(world.entities->settlements.territories[freeIndex], (world_entity_settlements::territory
            {
                .bounds_min = world.entities->settlements.locations[freeIndex],
                .bounds_max = world.entities->settlements.locations[freeIndex],
                .pressure = 0,
            }));

    // Claim own tile and set it as border to begin influence field.
    ui16 regionIndex, tileIndex;
    world_get_tile(*world.terrain, start_state.location, regionIndex, tileIndex);

    world_influence_claim_tile(world, regionIndex, tileIndex, freeIndex);

    // A settlement's tile counts as a road. Left as is if the settlement is destroyed.
    world.terrain->regions[regionIndex].status[tileIndex].is_road = true;

	return newID;
}

void world_destroy_settlement(match_world_state& world, entity_guid settlement)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	world_entity_settlements& settlements = world.entities->settlements;

	settlements.guids[settlement.get_index()] = INVALID_ENTITY_GUID;
	settlements.active_count--;
}

bool world_alloc_settlements(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_settlements& settlements = world.entities->settlements;

	settlements.max_count = count;
	settlements.active_count = 0;

	settlements.guids = memory.alloc<entity_guid>(count);
	if (settlements.guids == nullptr) return false;

	settlements.owners = memory.alloc<match_player_id>(count);
	if (settlements.owners == nullptr) return false;

	settlements.locations = memory.alloc<world_location>(count);
	if (settlements.locations == nullptr) return false;

    settlements.founding_ticks = memory.alloc<ui64>(count);
    if (settlements.founding_ticks == nullptr) return false;

	settlements.populations = memory.alloc<settlement_population>(count);
	if (settlements.populations == nullptr) return false;

	settlements.base_land_outputs = memory.alloc<ui32>(count);
	if (settlements.base_land_outputs == nullptr) return false;

	settlements.pending_incomes = memory.alloc<settlement_pending_income>(count);
	if (settlements.pending_incomes == nullptr) return false;

	settlements.tax_rates = memory.alloc<i8>(count);
	if (settlements.tax_rates == nullptr) return false;

	settlements.economies = memory.alloc<settlement_economy>(count);
	if (settlements.economies == nullptr) return false;

	settlements.buildings = memory.alloc<settlement_buildings>(count);
	if (settlements.buildings == nullptr) return false;

	settlements.trade = memory.alloc<settlement_trading>(count);
	if (settlements.trade == nullptr) return false;

	settlements.territories = memory.alloc<world_entity_settlements::territory>(count);
	if (settlements.territories == nullptr) return false;

	settlements.troop_ceilings = memory.alloc<army_troop_count>(count);
	if (settlements.troop_ceilings == nullptr) return false;

	return true;
}

bool world_entity_query_settlement(const match_world_state& world, entity_guid entity, world_entity_settlements::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::SETTLEMENT || !world_entity_is_valid(world, entity)) return false;

	const world_entity_settlements& settlements = world.entities->settlements;
	ui16 i = entity.get_index();

	out_state = {
		.owner = settlements.owners[i],
		.location = settlements.locations[i],
        .founding_tick = settlements.founding_ticks[i],
		.population = settlements.populations[i],
		.pending_income = settlements.pending_incomes[i],
		.tax_rate = settlements.tax_rates[i],
		.economy = settlements.economies[i],
		.territory = settlements.territories[i],
		.base_land_output = settlements.base_land_outputs[i],
        .buildings = settlements.buildings[i],
        .trade = settlements.trade[i],
		.troop_ceilings = settlements.troop_ceilings[i],
		.troop_effective_ceilings = settlement_troop_effective_ceilings(settlements.populations[i], settlements.troop_ceilings[i]),
	};
	return true;
}

void world_settlement_change_owner(match_world_state& world, ui16 settlement_index, match_player_id new_owner)
{
    // Population and soldiers (raised ones included) change hands with the settlement.
	world.entities->settlements.owners[settlement_index] = new_owner;

    // Reset tax rate to 0 / neutral.
	world.entities->settlements.tax_rates[settlement_index] = 0;
}

bool world_is_valid_settlement_location(const match_world_state& world, world_location loc, match_player_id player)
{
	const world_terrain& terrain = *world.terrain;
	if (loc.x >= terrain.size_tiles.x || loc.y >= terrain.size_tiles.y) return false;

	ui16 regionIndex, tileIndex;
	world_get_tile(terrain, loc, regionIndex, tileIndex);

	TERRAIN_TILE_TYPE type = terrain.regions[regionIndex].terrain_types[tileIndex];
	if (type != TERRAIN_TILE_TYPE::LAND_PLAINS && type != TERRAIN_TILE_TYPE::LAND_HILLS) return false;

	const world_entity_settlements& settlements = world.entities->settlements;

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


// SETTLEMENT BUILDING TYPES
// Type-specific event functions (see SETTLEMENT_BUILDING_TABLE in world_entities.h).

// BARRACKS

void settlement_building_barracks_on_added(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    ui16 settlementIndex = settlement.get_index();
    world.entities->settlements.populations[settlementIndex].troop_max.men_at_arms += GAME_PARAMETERS.barracks_maa_max_increase;
    // @TODO(Marc): Add enumeration for troop types.
    world.entities->settlements.populations[settlementIndex].troop_training[1].amount += 2;
}

void settlement_building_barracks_on_removed(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    ui16 settlementIndex = settlement.get_index();
    world.entities->settlements.populations[settlementIndex].troop_max.men_at_arms -= GAME_PARAMETERS.barracks_maa_max_increase;
    // @TODO(Marc): Add enumeration for troop types.
    world.entities->settlements.populations[settlementIndex].troop_training[1].amount -= 2;
}

// TRADE POSTS

// Export / import pool caps added by each trade post. Placeholders, may become building parameters later.
static constexpr ui32 TRADE_POST_EXPORT_POOL_CAPACITY = 100;
static constexpr ui32 TRADE_POST_IMPORT_POOL_CAPACITY = 200;

void settlement_building_tradepost_on_added(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    settlement_trading& trade = world.entities->settlements.trade[settlement.get_index()];
    trade.caravan_count_max++;
    trade.export_pool_max += TRADE_POST_EXPORT_POOL_CAPACITY;
    trade.import_pool_max += TRADE_POST_IMPORT_POOL_CAPACITY;
}

void settlement_building_tradepost_on_removed(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    // The pools themselves are kept: a cap below the contents only stops further adds.
    settlement_trading& trade = world.entities->settlements.trade[settlement.get_index()];
    trade.caravan_count_max--;
    trade.export_pool_max -= TRADE_POST_EXPORT_POOL_CAPACITY;
    trade.import_pool_max -= TRADE_POST_IMPORT_POOL_CAPACITY;
}
// ...
