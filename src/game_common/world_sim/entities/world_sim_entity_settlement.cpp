#include "world_sim_entity_settlement.h"
#include "game_common/match/game_parameters.h"
#include "game_common/match/world/entities/world_entity_settlements.h"
#include "game_common/match/world/world.h"
#include "game_common/match/world/world_terrain.h"

entity_guid world_spawn_settlement(match_world_state& world, const world_entity_settlements::single& start_state)
{
	world_entity_settlements& settlements = world.entities->settlements;

	ui16 freeIndex = 0;
	while (freeIndex < settlements.max_count && settlements.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < settlements.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::SETTLEMENT, freeIndex, world.age % 0x1000); // 12-bit extra field.

	world_entity_settlement_view newSettlement { settlements, freeIndex };
	newSettlement.copy_from(start_state);
	newSettlement.founding_tick = world.age;

	// Init default values if they weren't specified by start state.
	SET_DEFAULT_VAL(newSettlement.pop_peasants, 100);
	SET_DEFAULT_VAL(newSettlement.pop_total, newSettlement.at_home());
	SET_DEFAULT_VAL(newSettlement.pop_peasants_cap, newSettlement.pop_peasants);
	SET_DEFAULT_VAL(newSettlement.trade.market_goods, (ui32)((ui64)newSettlement.pop_total * GAME_PARAMETERS.trade.start_goods_percent / 100));
	SET_DEFAULT_VAL_STRUCT(newSettlement.territory, (world_entity_settlements::territory
		{
			.bounds_min = newSettlement.location,
			.bounds_max = newSettlement.location,
			.tile_count = 0,
		}));

	settlements.guids[freeIndex] = newID;
	settlements.active_count++;

	// Claim own tile and set it as border to begin influence field.
	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, newSettlement.location, regionIndex, tileIndex);
	world_influence_claim_tile(world, regionIndex, tileIndex, freeIndex);

	// A settlement's tile counts as a road. Left as is if the settlement is destroyed.
	world.terrain->regions[regionIndex].status[tileIndex].is_road = true;

	return newID;
}

void world_destroy_settlement(match_world_state& world, entity_guid settlement)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	world_entity_settlements& settlements = world.entities->settlements;

	ui16 index = settlement.get_index();
	world_entity_settlement_view destroyedSettlement { settlements, index };
	destroyedSettlement.copy_from(world_entity_settlement {});
	settlements.guids[index] = INVALID_ENTITY_GUID;
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

	settlements.pop_total = memory.alloc<ui32>(count);
	if (settlements.pop_total == nullptr) return false;

	settlements.pop_unemployed = memory.alloc<ui32>(count);
	if (settlements.pop_unemployed == nullptr) return false;

	settlements.pop_peasants = memory.alloc<ui32>(count);
	if (settlements.pop_peasants == nullptr) return false;

	settlements.pop_peasants_cap = memory.alloc<ui32>(count);
	if (settlements.pop_peasants_cap == nullptr) return false;

	settlements.pop_workers = memory.alloc<ui32>(count);
	if (settlements.pop_workers == nullptr) return false;

	settlements.garrison = memory.alloc<army_troop_count>(count);
	if (settlements.garrison == nullptr) return false;

	settlements.raised = memory.alloc<army_troop_count>(count);
	if (settlements.raised == nullptr) return false;

	settlements.troop_max = memory.alloc<army_troop_count>(count);
	if (settlements.troop_max == nullptr) return false;

	settlements.troop_training = memory.alloc<settlement_troop_training[4]>(count);
	if (settlements.troop_training == nullptr) return false;

	settlements.pop_change = memory.alloc<i32>(count);
	if (settlements.pop_change == nullptr) return false;

	settlements.pop_growth_per_mille = memory.alloc<i32>(count);
	if (settlements.pop_growth_per_mille == nullptr) return false;

	settlements.base_land_surplus = memory.alloc<ui32>(count);
	if (settlements.base_land_surplus == nullptr) return false;

	settlements.tax_rates = memory.alloc<i8>(count);
	if (settlements.tax_rates == nullptr) return false;

	settlements.buildings = memory.alloc<settlement_buildings>(count);
	if (settlements.buildings == nullptr) return false;

	settlements.trade = memory.alloc<settlement_trading>(count);
	if (settlements.trade == nullptr) return false;

	settlements.economies = memory.alloc<settlement_economy>(count);
	if (settlements.economies == nullptr) return false;

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
	ui16 index = entity.get_index();

	out_state.copy_from(settlements, index);
	out_state.market_capacity = settlement_market_capacity(settlements, index);
	out_state.troop_effective_ceilings = settlement_troop_effective_ceilings(settlements.troop_max[index], settlements.troop_ceilings[index]);
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
		if (dx * dx + dy * dy < (i32)(GAME_PARAMETERS.minimum_settlement_distance * GAME_PARAMETERS.minimum_settlement_distance)) return false;
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
    world.entities->settlements.troop_max[settlementIndex].men_at_arms += GAME_PARAMETERS.barracks_maa_max_increase;
    // @TODO(Marc): Add enumeration for troop types.
    world.entities->settlements.troop_training[settlementIndex][1].amount += 2;
}

void settlement_building_barracks_on_removed(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    ui16 settlementIndex = settlement.get_index();
    world.entities->settlements.troop_max[settlementIndex].men_at_arms -= GAME_PARAMETERS.barracks_maa_max_increase;
    // @TODO(Marc): Add enumeration for troop types.
    world.entities->settlements.troop_training[settlementIndex][1].amount -= 2;
}

// TRADE POSTS

// Market goods capacity added by each trade post.
static constexpr ui32 TRADE_POST_MARKET_CAPACITY = 300;

void settlement_building_tradepost_on_added(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    settlement_trading& trade = world.entities->settlements.trade[settlement.get_index()];
    trade.caravan_count_max++;
    trade.market_capacity_buildings += TRADE_POST_MARKET_CAPACITY;
}

void settlement_building_tradepost_on_removed(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    settlement_trading& trade = world.entities->settlements.trade[settlement.get_index()];
    trade.caravan_count_max--;
    trade.market_capacity_buildings -= TRADE_POST_MARKET_CAPACITY;
}

// TRADE DEPOT

// Market goods capacity added by each trade depot.
static constexpr ui32 TRADE_DEPOT_MARKET_CAPACITY = 1400;

void settlement_building_tradedepot_on_added(match_world_state& world, entity_guid settlement)
{
    // @NOTE(Marc): This function makes it evident that a data-oriented system for buildings to increase / decrease various settlement properties
    // would work far better. For now let's keep it simple, and if it gets really bad I'll look into it. It could somehow hook into settlement GDOs,
    // but I'm worried about overhead of heaving to access the settlement values through that GDO...

    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    world.entities->settlements.trade[settlement.get_index()].market_capacity_buildings += TRADE_DEPOT_MARKET_CAPACITY;
}

void settlement_building_tradedepot_on_removed(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    world.entities->settlements.trade[settlement.get_index()].market_capacity_buildings -= TRADE_DEPOT_MARKET_CAPACITY;
}
