#include "world_sim_entity_army.h"
#include "../world_sim.h"
#include "game_common/match/world/troop_types.h"
#include "world_sim_entity_settlement.h"
#include "world_sim_entity_engagement.h"
#include "game_common/match/world/world.h"

entity_guid world_spawn_army(match_world_state& world, const world_entity_armies::single& start_state)
{
	world_entity_armies& armies = world.entities->armies;

	ui16 freeIndex = 0;
	while (freeIndex < armies.max_count && armies.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < armies.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::ARMY, freeIndex, world.age % 0x100); // 8-bit extra field.

	world_entity_armies::view newArmy { armies, freeIndex };
	newArmy.copy_from(start_state);
	newArmy.engagement = INVALID_ENTITY_GUID;
	newArmy.desertions = 0;

	// Init default values if they weren't specified by start state.
	SET_DEFAULT_VAL(newArmy.movement.travel_speed, (ui16)GAME_PARAMETERS.army_travel_speed);
	SET_DEFAULT_VAL_STRUCT(newArmy.movement.destination, start_state.location);
	newArmy.movement.move_target = start_state.location;

	// A new army stands at its destination (the default order is STATION), unless the start state gives another order.
	if (start_state.order.type == ARMY_ORDER_STATION) newArmy.order.location = newArmy.movement.destination;

	armies.guids[freeIndex] = newID;
	armies.active_count++;

	return newID;
}

void world_destroy_army(match_world_state& world, entity_guid army)
{
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	world_entity_armies& armies = world.entities->armies;

	ui16 index = army.get_index();
	entity_guid engagement = armies.engagements[index];
	world_entity_army_view destroyedArmy { armies, index };
	destroyedArmy.copy_from(world_entity_army {});
	armies.guids[index] = INVALID_ENTITY_GUID;
	armies.active_count--;

	// A siege whose last attacking army is gone ends with it.
	if (engagement.is_valid()) world_engagement_army_left(world, engagement);
}

void world_disband_army(match_world_state& world, entity_guid army, entity_guid settlement)
{
	ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	// Callers make sure the settlement is owned by the army's owner (the disband command refuses otherwise, GO_HOME deserts instead): the men enter its garrison.
	ASSERT(world.entities->settlements.owners[settlement.get_index()] == world.entities->armies.owners[army.get_index()]);
	army_troop_count men = world.entities->armies.compositions[army.get_index()];

	// Levies return as unemployed population (settlement_release_raised), the other types rejoin the local troops (garrison).
	for (ui8 troopType = 0; troopType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; troopType++)
	{
		settlement_release_raised(world, settlement.get_index(), (TROOP_TYPE)troopType, men.count[troopType]);
	}

	// Homecoming: gold goes to the owner's treasury. Supplies are split by homecoming_settlement_goods_percent: that share joins the settlement's local goods
	// (only if it still is the owner's), the rest counts as gold 1:1 in the treasury.
	ui16 armyIndex = army.get_index();
	match_player_id owner = world.entities->armies.owners[armyIndex];
	ui32 supplies = world.entities->armies.supplies[armyIndex];
	ui32 toSettlement = 0;
	if (world.entities->settlements.owners[settlement.get_index()] == owner)
	{
		toSettlement = (ui32)((ui64)supplies * ia_min(GAME_PARAMETERS.homecoming_settlement_goods_percent, (ui32)100) / 100);
		world.entities->settlements.trade[settlement.get_index()].market_goods += toSettlement;
	}
	world.player_states[owner].treasury += world.entities->armies.gold[armyIndex] + (supplies - toSettlement);

	world_destroy_army(world, army);
}

void world_army_take_casualties(match_world_state& world, entity_guid army, const army_troop_count& kills, army_troop_count& out_dead)
{
	world_entity_armies& armies = world.entities->armies;

	for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
	{
		out_dead.count[unitType] = army_remove_troops(world, army.get_index(), (TROOP_TYPE)unitType, (ui16)ia_min(kills.count[unitType], (ui32)0xFFFF));
	}

	// The dead also leave their home settlement (raised counts and total population), even if it changed owner. Skipped if the home is gone.
	entity_guid home = armies.home_settlements[army.get_index()];
	if (!home.is_valid() || home.get_type() != ENTITY_TYPE::SETTLEMENT || !world_entity_is_valid(world, home)) return;

	for (ui8 unitType = 0; unitType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; unitType++)
	{
		settlement_remove_soldiers_raised(world, home.get_index(), (TROOP_TYPE)unitType, (ui16)out_dead.count[unitType]);
	}
}

ui16 world_army_desertion(match_world_state& world, entity_guid army, TROOP_TYPE troop_type, ui16 count)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 deserters = army_remove_troops(world, army.get_index(), troop_type, count);
	if (deserters == 0) return 0;

	// An army in a engagement no longer counts them among its side's men.
	entity_guid engagement = armies.engagements[army.get_index()];
	if (engagement.is_valid()) world_engagement_remove_troops(world, engagement, armies.engagement_sides[army.get_index()], troop_type, deserters);

	// They go back to the home settlement, even if it changed owner. Gone if the home no longer exists.
	entity_guid home = armies.home_settlements[army.get_index()];
	if (home.is_valid() && home.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, home))
	{
		// Those the settlement cannot take back (raised count too low) are lost.
		ui16 returned = settlement_release_raised(world, home.get_index(), troop_type, deserters);
		settlement_remove_soldiers_raised(world, home.get_index(), troop_type, deserters - returned);
	}

	// The last man leaving ends the army. The army is invalid after this.
	if (army_total_manpower(armies.compositions[army.get_index()]) == 0) world_destroy_army(world, army);
	return deserters;
}

void army_order_station_nearest_friendly_settlement(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;

	const world_entity_settlements& settlements = world.entities->settlements;
	ui32 nearest = settlements.max_count;
	float nearestDist = 0;
	for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid() || !world_entity_is_friendly(world, settlements.guids[settlementIndex], armies.owners[army_index])) continue;

		float dist = vec2_dist_squared(armies.locations[army_index], settlements.locations[settlementIndex]);
		if (nearest == settlements.max_count || dist < nearestDist)
		{
			nearest = settlementIndex;
			nearestDist = dist;
		}
	}
	// None left: it stands where it leaves the engagement.
	if (nearest < settlements.max_count) army_order_station(armies, army_index, settlements.locations[nearest]);
	else army_order_station(armies, army_index, armies.locations[army_index]);
}

bool world_entity_query_army(const match_world_state& world, entity_guid entity, world_entity_armies::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::ARMY || !world_entity_is_valid(world, entity)) return false;

	const world_entity_armies& armies = world.entities->armies;
	ui16 i = entity.get_index();

	out_state.copy_from(armies, i);
	out_state.supplies_consumption = army_daily_consumption(armies.compositions[i]);
	out_state.supplies_capacity = army_supply_capacity(world, i);
	return true;
}

bool world_alloc_armies(match_world_state& world, mem_arena& memory, ui32 count)
{
	world_entity_armies& armies = world.entities->armies;

	armies.max_count = count;

	armies.guids = memory.alloc<entity_guid>(count);
	if (armies.guids == nullptr) return false;

	armies.owners = memory.alloc<match_player_id>(count);
	if (armies.owners == nullptr) return false;

	armies.locations = memory.alloc<world_location>(count);
	if (armies.locations == nullptr) return false;

	armies.movements = memory.alloc<world_entity_movement>(count);
	if (armies.movements == nullptr) return false;

	armies.compositions = memory.alloc<army_troop_count>(count);
	if (armies.compositions == nullptr) return false;

	armies.engagements = memory.alloc<entity_guid>(count);
	if (armies.engagements == nullptr) return false;

	armies.engagement_sides = memory.alloc<ENGAGEMENT_SIDE>(count);
	if (armies.engagement_sides == nullptr) return false;

	armies.raised_troops = memory.alloc<army_troop_count>(count);
	if (armies.raised_troops == nullptr) return false;

	armies.raising = memory.alloc<ui8>(count);
	if (armies.raising == nullptr) return false;

	armies.home_settlements = memory.alloc<entity_guid>(count);
	if (armies.home_settlements == nullptr) return false;

	armies.orders = memory.alloc<army_order>(count);
	if (armies.orders == nullptr) return false;

	armies.gold = memory.alloc<ui32>(count);
	if (armies.gold == nullptr) return false;

	armies.supplies = memory.alloc<ui32>(count);
	if (armies.supplies == nullptr) return false;

	armies.desertions = memory.alloc<ui16>(count);
	if (armies.desertions == nullptr) return false;

	return true;
}

// What the given soldiers consume per day: sum of count x per-type upkeep rate. Levies are per mille on the levy total, rounded down.
ui32 army_daily_consumption(const army_troop_count& troops)
{
	ui64 consumption = ((ui64)troops.levies * GAME_PARAMETERS.upkeep_levy_per_mille) / 1000
		+ (ui64)troops.men_at_arms * GAME_PARAMETERS.upkeep_men_at_arms
		+ (ui64)troops.horsemen * GAME_PARAMETERS.upkeep_horseman
		+ (ui64)troops.knights * GAME_PARAMETERS.upkeep_knight;
	return (ui32)ia_min(consumption, (ui64)0xFFFFFFFF);
}

// The troops supplies are sized for: the raise targets while the army is raising, its composition after that.
const army_troop_count& army_supply_basis(const match_world_state& world, ui16 army_index)
{
	const world_entity_armies& armies = world.entities->armies;
	return armies.raising[army_index] ? armies.raised_troops[army_index] : armies.compositions[army_index];
}

// What the army carries at most, in each of its two pools (supplies, gold): sum of count x per-type carry capacity of its supply basis.
ui32 army_supply_capacity(const match_world_state& world, ui16 army_index)
{
	const army_troop_count& troops = army_supply_basis(world, army_index);
	ui64 capacity = (ui64)troops.levies * GAME_PARAMETERS.carry_levy
		+ (ui64)troops.men_at_arms * GAME_PARAMETERS.carry_men_at_arms
		+ (ui64)troops.horsemen * GAME_PARAMETERS.carry_horseman
		+ (ui64)troops.knights * GAME_PARAMETERS.carry_knight;
	return (ui32)ia_min(capacity, (ui64)0xFFFFFFFF);
}

// RAID LOOT (temporary: becomes tile-based raiding later)

// What is left of a settlement's daily share: day_max is the share of the amount the settlement had before today's looting, taken is what was already taken today.
static ui32 raid_budget_left(ui64 day_max, ui32 taken)
{
	return day_max > taken ? (ui32)(day_max - taken) : 0;
}

void world_army_loot_settlement(match_world_state& world, entity_guid army, entity_guid settlement)
{
	world_entity_armies& armies = world.entities->armies;
	world_entity_settlements& settlements = world.entities->settlements;
	ui16 army_index = army.get_index();
	ui16 settlement_index = settlement.get_index();
	settlement_raid& raid = settlements.raids[settlement_index];
	settlement_trading& trade = settlements.trade[settlement_index];
	const game_parameters& params = GAME_PARAMETERS;

	raid.raided = 1;
	raid.looted_today = 1;

	ui32 capacity = army_supply_capacity(world, army_index);
	ui32& gold = armies.gold[army_index];
	ui32& supplies = armies.supplies[army_index];
	ui32 goldRoom = capacity > gold ? capacity - gold : 0;
	ui32 suppliesRoom = capacity > supplies ? capacity - supplies : 0;

	// Production (last day's figure, the economy tick runs right after and does not credit what is taken).
	ui64 productionMax = (ui64)settlements.economies[settlement_index].production * ia_min(params.raid_production_percent, (ui32)100) / 100;
	ui64 goldMax = productionMax * ia_min(params.raid_production_gold_percent, (ui32)100) / 100;
	ui32 takenGold = ia_min(raid_budget_left(goldMax, raid.production_gold), goldRoom);
	ui32 takenSupplies = ia_min(raid_budget_left(productionMax - goldMax, raid.production_supplies), suppliesRoom);
	raid.production_gold += takenGold;
	raid.production_supplies += takenSupplies;
	gold += takenGold;
	supplies += takenSupplies;
	goldRoom -= takenGold;
	suppliesRoom -= takenSupplies;

	// Peasants killed, 1 gold each.
	ui64 killedMax = ((ui64)settlements.pop_peasants[settlement_index] + raid.peasants_killed) * ia_min(params.raid_peasants_percent, (ui32)100) / 100;
	ui32 killed = settlement_remove_peasants(world, settlement_index, ia_min(raid_budget_left(killedMax, raid.peasants_killed), goldRoom));
	raid.peasants_killed += killed;
	gold += killed;

	// Goods, local pool first, 1 supply each.
	ui64 goodsStock = (ui64)trade.market_goods + trade.export_goods;
	ui64 goodsMax = (goodsStock + raid.goods_taken) * ia_min(params.raid_goods_percent, (ui32)100) / 100;
	ui32 goods = (ui32)ia_min(ia_min((ui64)raid_budget_left(goodsMax, raid.goods_taken), (ui64)suppliesRoom), goodsStock);
	ui32 fromLocal = ia_min(goods, trade.market_goods);
	trade.market_goods -= fromLocal;
	trade.export_goods -= goods - fromLocal;
	raid.goods_taken += goods;
	supplies += goods;
}

void world_system_raid_loot(match_world_state& world)
{
	if (!world_is_daily_tick(world)) return;

	world_entity_armies& armies = world.entities->armies;
	world_entity_settlements& settlements = world.entities->settlements;

	for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		settlement_raid& raid = settlements.raids[settlementIndex];
		raid = { .raided = raid.looted_today };
	}

	if (armies.max_count == 0) return;
	ui32 start = world.rand_gen.next_range<ui32>(0, armies.max_count);
	for (ui32 step = 0; step < armies.max_count; step++)
	{
		ui16 armyIndex = (ui16)((start + step) % armies.max_count);
		if (!armies.guids[armyIndex].is_valid() || armies.raising[armyIndex] || armies.engagements[armyIndex].is_valid()) continue;

		const army_order& order = armies.orders[armyIndex];
		if (order.type != ARMY_ORDER_RAID || order.state != ARMY_ORDER_STATE_RAID_RAIDING) continue;
		if (!world_entity_is_valid(world, order.target) || order.target.get_type() != ENTITY_TYPE::SETTLEMENT) continue;

		world_army_loot_settlement(world, armies.guids[armyIndex], order.target);

		// Gold pool full: head home (GO_HOME disbands at home, or falls back to station if there is no valid home).
		ui32 capacity = army_supply_capacity(world, armyIndex);
		if (capacity > 0 && armies.gold[armyIndex] >= capacity)
		{
			armies.orders[armyIndex] = {};
			armies.orders[armyIndex].type = ARMY_ORDER_GO_HOME;
		}
	}
}
