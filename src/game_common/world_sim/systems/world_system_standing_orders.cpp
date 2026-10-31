// Standing orders of armies: each tick, what an army moves towards and what it does on arrival, from its engagement or its order (ARMY_ORDER_TYPE).
// An army pinned in an engagement walks to the engagement's location whatever its order says (the order is not worked on meanwhile and goes on afterwards).
// Raising armies ignore their order until raising ends. The order logic of each type is a row of ARMY_ORDER_DEFS.
// Hostility is world_entity_is_hostile. Targets are found by scanning all armies (no spatial index yet). Tunables are in GAME_PARAMETERS (order_*).

#include "game_common/match/world/world_state.h"

#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_engagement.h"
#include "../entities/world_sim_entity_settlement.h"

#include "../world_movement.h"

// Order logic: one function per order type, run once per tick before movement, for armies that are not raising or pinned in an engagement.
// It handles arrival first (the army is close enough to what it was going for, from its position after the last movement), then sets the movement
// destination (order_travel) and replaces the order if it became invalid. Gets the army's slot index.
struct army_order_def
{
	const char* name;
	void (*logic)(match_world_state& world, ui16 army_index);
};

// HELPERS

// An order that cannot go on: the army stands where it is.
static void army_order_fall_back_to_station(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order_station(armies, army_index, armies.locations[army_index]);
	world_movement_stop(armies.movements[army_index], armies.locations[army_index]);
}

static bool order_tile_owned_by(const match_world_state& world, world_location location, ui16 settlement_index)
{
	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, location, regionIndex, tileIndex);
	return world.terrain->regions[regionIndex].owners[tileIndex] == settlement_index;
}

// Plains or hills: where an army can be sent to stand.
static bool order_tile_is_standable(const match_world_state& world, world_location location)
{
	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, location, regionIndex, tileIndex);
	TERRAIN_TILE_TYPE type = world.terrain->regions[regionIndex].terrain_types[tileIndex];
	return type == TERRAIN_TILE_TYPE::LAND_PLAINS || type == TERRAIN_TILE_TYPE::LAND_HILLS;
}

// A random standable tile of the settlement's territory within radius tiles of the center. Falls back to the center after a few tries.
static world_location order_pick_point(match_world_state& world, ui16 settlement_index, world_location center, i32 radius)
{
	const world_dimensions& size = world.terrain->size_tiles;
	for (ui32 attempt = 0; attempt < 8; attempt++)
	{
		i32 x = (i32)center.x + (i32)(world.rand_gen.next<ui32>() % (ui32)(2 * radius + 1)) - radius;
		i32 y = (i32)center.y + (i32)(world.rand_gen.next<ui32>() % (ui32)(2 * radius + 1)) - radius;
		if (x < 0 || y < 0 || x >= (i32)size.x || y >= (i32)size.y) continue;

		world_location point = { (ui16)x, (ui16)y };
		if (order_tile_is_standable(world, point) && order_tile_owned_by(world, point, settlement_index)) return point;
	}
	return center;
}

static bool order_is_hostile_army(const match_world_state& world, ui16 army_index, ui16 other_index)
{
	const world_entity_armies& armies = world.entities->armies;
	return armies.guids[other_index].is_valid() && world_entity_is_hostile(world, armies.guids[other_index], armies.owners[army_index])
		&& army_total_manpower(armies.compositions[other_index]) > 0;
}

// The hostile army nearest to the center within the radius (lowest slot on a tie), or an invalid guid. With territory_of set, only armies inside that settlement's territory.
static entity_guid order_find_hostile_army(match_world_state& world, ui16 army_index, world_location center, float radius, ui16 territory_of)
{
	const world_entity_armies& armies = world.entities->armies;
	entity_guid found = INVALID_ENTITY_GUID;
	float foundDist = 0;
	for (ui32 other = 0; other < armies.max_count; other++)
	{
		if (!order_is_hostile_army(world, army_index, (ui16)other)) continue;

		float dist = vec2_dist_squared(center, armies.locations[other]);
		if (dist > radius * radius) continue;
		if (found.is_valid() && dist >= foundDist) continue;
		if (territory_of != 0xFFFF && !order_tile_owned_by(world, armies.locations[other], territory_of)) continue;

		found = armies.guids[other];
		foundDist = dist;
	}
	return found;
}

// Total men of the hostile armies within the radius of the center, or inside the territory of territory_of (if set).
static ui64 order_hostile_men(match_world_state& world, ui16 army_index, world_location center, float radius, ui16 territory_of)
{
	const world_entity_armies& armies = world.entities->armies;
	ui64 men = 0;
	for (ui32 other = 0; other < armies.max_count; other++)
	{
		if (!order_is_hostile_army(world, army_index, (ui16)other)) continue;

		bool near = vec2_dist_squared(center, armies.locations[other]) <= radius * radius;
		bool inside = territory_of != 0xFFFF && order_tile_owned_by(world, armies.locations[other], territory_of);
		if (near || inside) men += army_total_manpower(armies.compositions[other]);
	}
	return men;
}

// Whether the entity is a hostile army still worth chasing.
static bool order_engaged_is_valid(match_world_state& world, ui16 army_index)
{
	const world_entity_armies& armies = world.entities->armies;
	entity_guid engaged = armies.orders[army_index].engaged;
	return engaged.is_valid() && world_entity_is_valid(world, engaged) && engaged.get_type() == ENTITY_TYPE::ARMY
		&& order_is_hostile_army(world, army_index, engaged.get_index());
}

static bool order_within_tiles(world_location a, world_location b, ui32 tiles)
{
	return vec2_dist_squared(a, b) <= (float)tiles * (float)tiles;
}

// Travel: sets the army's destination to the location. Returns whether it has arrived, meaning it is within the given tiles of it (from its position after the last movement).
static bool order_travel(match_world_state& world, ui16 army_index, world_location location, ui32 arrive_tiles)
{
	world_entity_armies& armies = world.entities->armies;
	armies.movements[army_index].destination = location;
	return order_within_tiles(armies.locations[army_index], location, arrive_tiles);
}

// Wander inside a settlement's territory (DEFEND_TERRITORY patrol, RAID): walk to order.location. A new point (within order_defend_wander_radius of the settlement)
// is picked at random while standing on the current one, or at once if it is not a point of the territory. Returns whether the army stands on its point.
static bool order_wander(match_world_state& world, ui16 army_index, ui16 settlement_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order& order = armies.orders[army_index];
	world_location settlementLocation = world.entities->settlements.locations[settlement_index];

	bool pointValid = order_tile_owned_by(world, order.location, settlement_index);
	if (!pointValid)
	{
		order.location = order_pick_point(world, settlement_index, settlementLocation, (i32)GAME_PARAMETERS.order_defend_wander_radius);
	}
	bool onPoint = order_travel(world, army_index, order.location, 0);
	if (pointValid && onPoint && world.rand_gen.next<ui32>() % ia_max(GAME_PARAMETERS.order_defend_wander_chance, 1u) == 0)
	{
		order.location = order_pick_point(world, settlement_index, settlementLocation, (i32)GAME_PARAMETERS.order_defend_wander_radius);
		onPoint = order_travel(world, army_index, order.location, 0);
	}
	return onPoint;
}

// STATION

// Go to the order's location. Hostile armies that come within the acquire radius are chased, as long as they stay within the leash of the station point.
static void army_order_station_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order& order = armies.orders[army_index];

	// Arrival: close enough to the chased army, the attack is resolved (an engagement starts or is joined, which ends the chase).
	if (order_engaged_is_valid(world, army_index)
		&& order_within_tiles(armies.locations[army_index], world_entity_get_location(world, order.engaged), GAME_PARAMETERS.order_attack_reach_tiles))
	{
		entity_guid target = order.engaged;
		order.engaged = INVALID_ENTITY_GUID;
		world_movement_stop(armies.movements[army_index], armies.locations[army_index]);
		world_resolve_attack(world, armies.guids[army_index], target);
		return;
	}

	if (order_engaged_is_valid(world, army_index))
	{
		world_location engagedLocation = world_entity_get_location(world, order.engaged);
		if (order_within_tiles(order.location, engagedLocation, GAME_PARAMETERS.order_station_leash))
		{
			armies.movements[army_index].destination = engagedLocation;
			return;
		}
	}
	order.engaged = INVALID_ENTITY_GUID;

	order_travel(world, army_index, order.location, 0); // Arriving changes nothing: it keeps watching for hostile armies.

	entity_guid found = order_find_hostile_army(world, army_index, armies.locations[army_index], (float)GAME_PARAMETERS.order_station_acquire_radius, 0xFFFF);
	if (found.is_valid() && order_within_tiles(order.location, world_entity_get_location(world, found), GAME_PARAMETERS.order_station_leash))
	{
		order.engaged = found;
		armies.movements[army_index].destination = world_entity_get_location(world, found);
	}
	// @TODO(Marc): Caravans (hostile or neutral) within the acquire radius should be attacked too. There is no caravan attack resolution yet.
}

// ATTACK

// Chase the target's current location. Close enough to it, the attack is resolved (a siege or engagement starts or is joined, which ends the chase: STATION).
// The order ends (STATION) if the target is gone. A settlement that is not hostile anymore is defended instead.
static void army_order_attack_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid target = armies.orders[army_index].target;

	if (!world_entity_is_valid(world, target))
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	if (target.get_type() == ENTITY_TYPE::SETTLEMENT && !world_entity_is_hostile(world, target, armies.owners[army_index]))
	{
		army_order_defend_territory(armies, army_index, target, armies.locations[army_index]);
		return; // Worked on from the next tick.
	}

	// Always travelling: arriving ends the order.
	if (order_travel(world, army_index, world_entity_get_location(world, target), GAME_PARAMETERS.order_attack_reach_tiles))
	{
		army_order_fall_back_to_station(world, army_index);
		world_resolve_attack(world, armies.guids[army_index], target);
	}
}

// DEFEND_TERRITORY (target = an own settlement)

// Patrol: walk between random points of the territory, attack hostile armies inside it. If the hostile armies in or near the territory outnumber the army, it retreats
// to the settlement tile and stands there, fighting what comes within the acquire radius, until none is left near.
static void army_order_defend_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order& order = armies.orders[army_index];
	entity_guid settlement = order.target;

	// Arrival: close enough to the chased army, the attack is resolved (an engagement starts or is joined, which ends the chase).
	if (order_engaged_is_valid(world, army_index)
		&& order_within_tiles(armies.locations[army_index], world_entity_get_location(world, order.engaged), GAME_PARAMETERS.order_attack_reach_tiles))
	{
		entity_guid target = order.engaged;
		order.engaged = INVALID_ENTITY_GUID;
		world_movement_stop(armies.movements[army_index], armies.locations[army_index]);
		world_resolve_attack(world, armies.guids[army_index], target);
		return;
	}

	// The settlement is gone or lost: nothing to defend.
	if (!world_entity_is_valid(world, settlement) || settlement.get_type() != ENTITY_TYPE::SETTLEMENT
		|| world_entity_is_hostile(world, settlement, armies.owners[army_index]))
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	ui16 settlementIndex = settlement.get_index();
	world_location settlementLocation = world.entities->settlements.locations[settlementIndex];
	ui64 threat = order_hostile_men(world, army_index, settlementLocation, (float)GAME_PARAMETERS.order_defend_threat_radius, settlementIndex);
	ui64 mine = army_total_manpower(armies.compositions[army_index]);

	// TRAVELLING (outside the territory) patrols like PATROL does: intruders are attacked on the way.
	if (order.state != ARMY_ORDER_STATE_DEFEND_RETREATED && threat > mine)
	{
		order.state = ARMY_ORDER_STATE_DEFEND_RETREATED;
		order.engaged = INVALID_ENTITY_GUID;
	}
	else if (order.state == ARMY_ORDER_STATE_DEFEND_RETREATED && threat == 0)
	{
		order.state = ARMY_ORDER_STATE_TRAVELLING; // Resolved below.
		order.engaged = INVALID_ENTITY_GUID;
	}

	// The objective area is the territory: travelling while outside it, patrolling inside it (wandering included).
	if (order.state != ARMY_ORDER_STATE_DEFEND_RETREATED)
		order.state = order_tile_owned_by(world, armies.locations[army_index], settlementIndex) ? ARMY_ORDER_STATE_DEFEND_PATROL : ARMY_ORDER_STATE_TRAVELLING;

	if (order.state == ARMY_ORDER_STATE_DEFEND_RETREATED)
	{
		// Stand on the settlement tile, fight what comes close.
		order.location = settlementLocation;
		if (order_engaged_is_valid(world, army_index))
		{
			armies.movements[army_index].destination = world_entity_get_location(world, order.engaged);
			return;
		}
		order.engaged = order_find_hostile_army(world, army_index, armies.locations[army_index], (float)GAME_PARAMETERS.order_station_acquire_radius, 0xFFFF);
		if (order.engaged.is_valid()) armies.movements[army_index].destination = world_entity_get_location(world, order.engaged);
		else order_travel(world, army_index, settlementLocation, 0);
		return;
	}

	// Patrol: intruders first (only while they are inside the territory).
	if (order_engaged_is_valid(world, army_index) && order_tile_owned_by(world, world_entity_get_location(world, order.engaged), settlementIndex))
	{
		armies.movements[army_index].destination = world_entity_get_location(world, order.engaged);
		return;
	}
	order.engaged = order_find_hostile_army(world, army_index, armies.locations[army_index], 1e9f, settlementIndex);
	if (order.engaged.is_valid())
	{
		armies.movements[army_index].destination = world_entity_get_location(world, order.engaged);
		return;
	}

	// No intruder: wander.
	order_wander(world, army_index, settlementIndex);
}

// RAID (target = a hostile settlement)

// Go to the target's territory, then raid it while wandering inside. Flees stronger hostile armies close by (towards the home settlement) and comes back when none is near.
static void army_order_raid_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order& order = armies.orders[army_index];
	entity_guid target = order.target;

	// The target is gone or not hostile anymore.
	if (!world_entity_is_valid(world, target) || target.get_type() != ENTITY_TYPE::SETTLEMENT
		|| !world_entity_is_hostile(world, target, armies.owners[army_index]))
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	ui16 targetIndex = target.get_index();
	world_location targetLocation = world.entities->settlements.locations[targetIndex];
	ui64 mine = army_total_manpower(armies.compositions[army_index]);

	// Flees in every state but FLEEING.
	if (order.state != ARMY_ORDER_STATE_RAID_FLEEING
		&& order_hostile_men(world, army_index, armies.locations[army_index], (float)GAME_PARAMETERS.order_raid_flee_radius, 0xFFFF) > mine)
		order.state = ARMY_ORDER_STATE_RAID_FLEEING;
	else if (order.state == ARMY_ORDER_STATE_RAID_FLEEING
		&& order_hostile_men(world, army_index, armies.locations[army_index], (float)GAME_PARAMETERS.order_raid_return_radius, 0xFFFF) == 0)
		order.state = ARMY_ORDER_STATE_TRAVELLING; // Resolved below: walks back in, or resumes raiding if inside the territory.

	if (order.state == ARMY_ORDER_STATE_RAID_FLEEING)
	{
		// Run to the home settlement. Without one, stand and take what comes.
		entity_guid home = armies.home_settlements[army_index];
		if (world_entity_is_valid(world, home) && home.get_type() == ENTITY_TYPE::SETTLEMENT && world.entities->settlements.owners[home.get_index()] == armies.owners[army_index])
			order_travel(world, army_index, world.entities->settlements.locations[home.get_index()], 0); // Keeps running until the threat is gone.
		else
			army_order_fall_back_to_station(world, army_index);
		return;
	}

	// Outside the territory: travel to the settlement until inside it. Inside: raiding starts, wandering the territory like a defending army does
	// (RAID_MOVING while walking to its point, RAID_RAIDING while standing on it).
	if (!order_tile_owned_by(world, armies.locations[army_index], targetIndex))
	{
		order.state = ARMY_ORDER_STATE_TRAVELLING;
		order_travel(world, army_index, targetLocation, 0);
		return;
	}
	order.state = order_wander(world, army_index, targetIndex) ? ARMY_ORDER_STATE_RAID_RAIDING : ARMY_ORDER_STATE_RAID_MOVING;

	// Looting is world_system_raid_loot (daily).
	// @TODO(Marc): Enemy / neutral caravans close by should be attacked too. There is no caravan attack resolution yet.
}

// RAID LOOT (temporary: becomes tile-based raiding later)

// What is left of a settlement's daily share: day_max is the share of the amount the settlement had before today's looting, taken is what was already taken today.
static ui32 raid_budget_left(ui64 day_max, ui32 taken)
{
	return day_max > taken ? (ui32)(day_max - taken) : 0;
}

// One army loots the settlement it raids: production (gold and supplies), peasants killed (gold), goods (supplies). Each is the min of the daily potential left on the
// settlement and the room left in the army's gold / supplies pool (capacity: army_supply_capacity). What is taken leaves the settlement; nothing else is destroyed.
static void army_loot_settlement(match_world_state& world, ui16 army_index, ui16 settlement_index)
{
	world_entity_armies& armies = world.entities->armies;
	world_entity_settlements& settlements = world.entities->settlements;
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

// Runs daily, before the settlement economy. Resets the settlements' daily raided totals (raided = looted yesterday), then every army raiding (RAID_RAIDING or RAID_MOVING,
// not fleeing / travelling / raising / in an engagement) loots its target. Armies go in index order from a random start so none has a standing advantage on a shared target.
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
		if (order.type != ARMY_ORDER_RAID || (order.state != ARMY_ORDER_STATE_RAID_RAIDING && order.state != ARMY_ORDER_STATE_RAID_MOVING)) continue;
		if (!world_entity_is_valid(world, order.target) || order.target.get_type() != ENTITY_TYPE::SETTLEMENT) continue;

		army_loot_settlement(world, armyIndex, order.target.get_index());
	}
}

// GO_HOME

// Walk to the home settlement. Close enough to it (and inside its territory, like the disband command requires), the army disbands into it.
// Without a home settlement of the owner, the army stands where it is.
static void army_order_go_home_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid home = armies.home_settlements[army_index];

	if (!world_entity_is_valid(world, home) || home.get_type() != ENTITY_TYPE::SETTLEMENT || world_entity_is_hostile(world, home, armies.owners[army_index]))
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	ui16 homeIndex = home.get_index();
	world_location homeLocation = world.entities->settlements.locations[homeIndex];
	// Always travelling: arriving ends the order.
	if (order_travel(world, army_index, homeLocation, GAME_PARAMETERS.order_go_home_disband_tiles)
		&& order_tile_owned_by(world, armies.locations[army_index], homeIndex))
		world_disband_army(world, armies.guids[army_index], home);
}

// One row per ARMY_ORDER_TYPE, in order.
static const army_order_def ARMY_ORDER_DEFS[ARMY_ORDER_TYPE_COUNT] =
{
	{ "STATION", army_order_station_logic },
	{ "ATTACK", army_order_attack_logic },
	{ "DEFEND_TERRITORY", army_order_defend_logic },
	{ "RAID", army_order_raid_logic },
	{ "GO_HOME", army_order_go_home_logic },
};

void world_system_army_orders(match_world_state& world)
{
	world_entity_armies& armies = world.entities->armies;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;

		// Armies in an engagement walk to its location and are pinned once close enough. This wins over the order.
		if (armies.engagements[armyIndex].is_valid())
		{
			world_location engagementLoc = world.entities->engagements.states[armies.engagements[armyIndex].get_index()].location;
			armies.movements[armyIndex].destination = vec2_dist_squared(armies.locations[armyIndex], engagementLoc) > ENGAGEMENT_ARMY_DISTANCE * ENGAGEMENT_ARMY_DISTANCE
				? engagementLoc : armies.locations[armyIndex];
			continue;
		}

		// Raising armies stay where they were raised: their order (set by the raise command) starts when raising ends.
		if (armies.raising[armyIndex]) continue;

		const army_order_def* def = armies.orders[armyIndex].type < ARMY_ORDER_TYPE_COUNT ? &ARMY_ORDER_DEFS[armies.orders[armyIndex].type] : nullptr;
		if (def == nullptr || def->logic == nullptr)
		{
			army_order_fall_back_to_station(world, (ui16)armyIndex);
			continue;
		}
		def->logic(world, (ui16)armyIndex);
	}
}
