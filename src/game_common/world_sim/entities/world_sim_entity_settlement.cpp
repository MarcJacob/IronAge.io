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
    if (newSettlement.at_home() == 0)
    {
	    newSettlement.pop_unemployed = 100; // 100 Unemployed population if no starting population of any kind was given.
    }
    SET_DEFAULT_VAL(newSettlement.pop_urban_cap, 100);
	SET_DEFAULT_VAL(newSettlement.pop_total, newSettlement.at_home());
	newSettlement.troop_max.levies = settlement_levy_max(newSettlement.pop_total);
	newSettlement.garrison.levies = ia_min(newSettlement.garrison.levies, newSettlement.troop_max.levies);
	SET_DEFAULT_VAL(newSettlement.pop_peasants_cap, newSettlement.pop_peasants);
	SET_DEFAULT_VAL(newSettlement.trade.export_ratio, (ui8)ia_min(GAME_PARAMETERS.trade.start_export_ratio_percent, (ui32)100));
	SET_DEFAULT_VAL(newSettlement.trade.caravan_count_max, (ui8)ia_min(GAME_PARAMETERS.trade.start_caravan_slots, (ui32)0xFF));
	// Default goods only if the start state gives none in either pool. Split by the export ratio like any income (set above).
	if (newSettlement.trade.market_goods == 0 && newSettlement.trade.export_goods == 0)
	{
		settlement_market_add(settlements, freeIndex, (ui64)newSettlement.pop_total * GAME_PARAMETERS.trade.start_goods_percent / 100);
	}
	SET_DEFAULT_VAL_STRUCT(newSettlement.territory, (world_entity_settlements::settlement_territory
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

	settlements.owner = memory.alloc<match_player_id>(count);
	if (settlements.owner == nullptr) return false;

	settlements.location = memory.alloc<world_location>(count);
	if (settlements.location == nullptr) return false;

    settlements.founding_tick = memory.alloc<ui64>(count);
    if (settlements.founding_tick == nullptr) return false;

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

	settlements.pop_urban_cap = memory.alloc<ui32>(count);
	if (settlements.pop_urban_cap == nullptr) return false;

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

	settlements.tax_rate = memory.alloc<i8>(count);
	if (settlements.tax_rate == nullptr) return false;

	settlements.buildings = memory.alloc<settlement_buildings>(count);
	if (settlements.buildings == nullptr) return false;

	settlements.trade = memory.alloc<settlement_trading>(count);
	if (settlements.trade == nullptr) return false;

	settlements.economy = memory.alloc<settlement_economy>(count);
	if (settlements.economy == nullptr) return false;

	settlements.territory = memory.alloc<world_entity_settlements::settlement_territory>(count);
	if (settlements.territory == nullptr) return false;

	settlements.troop_ceilings = memory.alloc<army_troop_count>(count);
	if (settlements.troop_ceilings == nullptr) return false;

	settlements.siege = memory.alloc<entity_guid>(count);
	if (settlements.siege == nullptr) return false;

	settlements.campaign = memory.alloc<entity_guid>(count);
	if (settlements.campaign == nullptr) return false;

	settlements.campaign_army = memory.alloc<entity_guid>(count);
	if (settlements.campaign_army == nullptr) return false;

	settlements.raid = memory.alloc<settlement_raid>(count);
	if (settlements.raid == nullptr) return false;

	return true;
}

bool world_entity_query_settlement(const match_world_state& world, entity_guid entity, world_entity_settlements::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::SETTLEMENT || !world_entity_is_valid(world, entity)) return false;

	const world_entity_settlements& settlements = world.entities->settlements;
	ui16 index = entity.get_index();

	out_state.copy_from(settlements, index);
	out_state.market_capacity = settlement_market_capacity(settlements, index);
	out_state.trade_volume_max = settlement_trade_volume_max(settlements, index);
	out_state.troop_effective_ceilings = settlement_troop_effective_ceilings(settlements.troop_max[index], settlements.troop_ceilings[index]);
	return true;
}

void world_settlement_change_owner(match_world_state& world, ui16 settlement_index, match_player_id new_owner)
{
    // Population and soldiers (raised ones included) change hands with the settlement.
	world.entities->settlements.owner[settlement_index] = new_owner;

    // Reset tax rate to 0 / neutral.
	world.entities->settlements.tax_rate[settlement_index] = 0;
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
	if (tileOwner != world_terrain::UNOWNED_TILE && settlements.owner[tileOwner] != player) return false;

	for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid()) continue;

		i32 dx = (i32)settlements.location[settlementIndex].x - (i32)loc.x;
		i32 dy = (i32)settlements.location[settlementIndex].y - (i32)loc.y;
		if (dx * dx + dy * dy < (i32)(GAME_PARAMETERS.minimum_settlement_distance * GAME_PARAMETERS.minimum_settlement_distance)) return false;
	}

	return true;
}

// Targets: a share of each type of the garrison.
army_troop_count world_settlement_campaign_army_targets(const world_entity_settlements& settlements, ui16 settlement_index)
{
	army_troop_count targets = {};
	for (ui8 troopType = 0; troopType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; troopType++)
	{
		targets.count[troopType] = (ui16)((ui32)settlements.garrison[settlement_index].count[troopType] * GAME_PARAMETERS.campaign_army_garrison_percent / 100);
	}
	return targets;
}

bool world_settlement_join_campaign(match_world_state& world, ui16 settlement_index, entity_guid campaign)
{
	world_entity_settlements& settlements = world.entities->settlements;
	world_entity_armies& armies = world.entities->armies;

	if (settlement_is_besieged(world, settlement_index)) return false;

	// The settlement kept the army of its previous campaign (campaign_army): it is sent to the new one with what it has (no cost, no new men).
	if (world_entity_is_valid(world, settlements.campaign_army[settlement_index]))
	{
		ui16 armyIndex = settlements.campaign_army[settlement_index].get_index();
		if (armies.owner[armyIndex] != settlements.owner[settlement_index]) return false; // Kept from before the settlement changed hands: not ours to send.
		armies.order[armyIndex] = {};
		armies.order[armyIndex].type = ARMY_ORDER_JOIN_CAMPAIGN;
		armies.order[armyIndex].target = campaign;
		armies.order[armyIndex].location = armies.location[armyIndex];
		return true;
	}

	if (armies.active_count >= armies.max_count) return false;

	army_troop_count targets = world_settlement_campaign_army_targets(settlements, settlement_index);
	if (army_total_manpower(targets) == 0) return false;

	// Same terms as the raise command: the army starts with army_raising_cost of gold from the treasury, and as much of it in goods from the market (the settlement keeps a day of its own).
	world_player_state& player = world.player_states[settlements.owner[settlement_index]];
	ui32 cost = army_raising_cost(targets);
	if (cost > player.treasury) return false;

	ui32 goodsKept = settlements.economy[settlement_index].consumption;
	settlement_trading& trade = settlements.trade[settlement_index];

	world_entity_armies::single newArmy = {};
	newArmy.owner = settlements.owner[settlement_index];
	newArmy.location = settlements.location[settlement_index];
	newArmy.raised_troops = targets;
	newArmy.home_settlement = settlements.guids[settlement_index];
	newArmy.raising = 1; // is_retinue stays 0: a campaign army, controlled by the campaign logic only.
	newArmy.gold = cost;
	newArmy.supplies = ia_min(cost, trade.market_goods > goodsKept ? trade.market_goods - goodsKept : 0);

	// The order it follows once it is raised (raising armies ignore their order): walk to the campaign.
	newArmy.order.type = ARMY_ORDER_JOIN_CAMPAIGN;
	newArmy.order.target = campaign;
	newArmy.order.location = newArmy.location;

	settlements.campaign_army[settlement_index] = world_spawn_army(world, newArmy);
	player.treasury -= cost;
	player.expenses += cost;
	trade.market_goods -= newArmy.supplies;
	return true;
}


// SETTLEMENT BUILDING TYPES
// Type-specific event functions (see SETTLEMENT_BUILDING_TABLE in world_entities.h).

// HOUSING

void settlement_building_housing_on_added(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    ui16 settlementIndex = settlement.get_index();
    world.entities->settlements.pop_urban_cap[settlementIndex] += GAME_PARAMETERS.housing_pop_increase;
}

void settlement_building_housing_on_removed(match_world_state& world, entity_guid settlement)
{
    ui16 settlementIndex = settlement.get_index();
    world.entities->settlements.pop_urban_cap[settlementIndex] -= GAME_PARAMETERS.housing_pop_increase;
}

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
    world.entities->settlements.troop_max[settlement.get_index()].wagons += GAME_PARAMETERS.tradepost_wagon_max_increase;
}

void settlement_building_tradepost_on_removed(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    settlement_trading& trade = world.entities->settlements.trade[settlement.get_index()];
    trade.caravan_count_max--;
    trade.market_capacity_buildings -= TRADE_POST_MARKET_CAPACITY;
    world.entities->settlements.troop_max[settlement.get_index()].wagons -= GAME_PARAMETERS.tradepost_wagon_max_increase;
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

// TRADE PORTS (placeholder: a trade post with a ship slot instead of a caravan slot)

// Market goods capacity added by each trade port.
static constexpr ui32 TRADE_PORT_MARKET_CAPACITY = 300;

void settlement_building_tradeport_on_added(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    settlement_trading& trade = world.entities->settlements.trade[settlement.get_index()];
    trade.ship_count_max++;
    trade.market_capacity_buildings += TRADE_PORT_MARKET_CAPACITY;
}

void settlement_building_tradeport_on_removed(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    settlement_trading& trade = world.entities->settlements.trade[settlement.get_index()];
    trade.ship_count_max--;
    trade.market_capacity_buildings -= TRADE_PORT_MARKET_CAPACITY;
}
