// Standing orders of armies: each tick, what an army moves towards and what it does on arrival, from its engagement or its order (ARMY_ORDER_TYPE).
// An army pinned in an engagement walks to the engagement's location whatever its order says (the order is not worked on meanwhile and goes on afterwards).
// Raising armies ignore their order until raising ends. The order logic of each type is a row of ARMY_ORDER_DEFS.
// Hostile = an army of another player (no diplomacy yet). Targets are found by scanning all armies (no spatial index yet).

#include "game_common/match/world/world_state.h"

#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_engagement.h"
#include "../entities/world_sim_entity_settlement.h"

#include "../world_movement.h"

// STATION: hostile armies this close to the army are attacked...
constexpr float ORDER_ACQUIRE_RADIUS = 5;
// ... as long as they are at most this far from the station point (the army returns to it afterwards).
constexpr float ORDER_STATION_LEASH = 10;

// DEFEND_TERRITORY: the army walks to random points within this many tiles of its settlement (inside the territory).
constexpr i32 ORDER_DEFEND_WANDER_RADIUS = 12;
// Each tick it stands on its point, it picks a new one with a chance of 1 in this.
constexpr ui32 ORDER_DEFEND_WANDER_CHANCE = 40;
// Hostile armies inside the territory, or this close to the settlement, count in the threat the army weighs itself against.
constexpr float ORDER_DEFEND_THREAT_RADIUS = 20;

// RAID: the army stands on a point within this many tiles of the target settlement (inside its territory).
constexpr i32 ORDER_RAID_POINT_RADIUS = 8;
// Hostile armies this close with more men than the army make it flee, until none is within the return radius.
constexpr float ORDER_RAID_FLEE_RADIUS = 15;
constexpr float ORDER_RAID_RETURN_RADIUS = 25;

// GO_HOME: the army disbands once this close to its home settlement.
constexpr float ORDER_GO_HOME_DISBAND_TILES = 2;

// Sub-states (army_order::state).
constexpr ui8 ORDER_DEFEND_PATROL = 0;
constexpr ui8 ORDER_DEFEND_RETREATED = 1; // Stands on the settlement tile, fights what comes close.
constexpr ui8 ORDER_RAID_RAIDING = 0;
constexpr ui8 ORDER_RAID_FLEEING = 1;

// Order logic. Both functions get the army's slot index; either can be null.
// decide: runs before movement, sets the movement destination and replaces the order if it became invalid.
// arrive: runs after movement, handles the army having reached what it was going for.
struct army_order_def
{
	const char* name;
	void (*decide)(match_world_state& world, ui16 army_index);
	void (*arrive)(match_world_state& world, ui16 army_index);
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
	return other_index != army_index && armies.guids[other_index].is_valid() && armies.owners[other_index] != armies.owners[army_index]
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

// Arrival of an order chasing an engaged army (STATION, DEFEND_TERRITORY): close enough to it, the attack is resolved (a engagement starts or is joined, which ends the chase).
// Same resolution as ATTACK.
static void army_order_engaged_arrive(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	if (!order_engaged_is_valid(world, army_index)) return;

	entity_guid target = armies.orders[army_index].engaged;
	if (vec2_dist_squared(armies.locations[army_index], world_entity_get_location(world, target)) > ATTACK_REACH_TILES * ATTACK_REACH_TILES) return;

	armies.orders[army_index].engaged = INVALID_ENTITY_GUID;
	world_movement_stop(armies.movements[army_index], armies.locations[army_index]);
	world_resolve_attack(world, armies.guids[army_index], target);
}

// STATION

// Go to the order's location. Hostile armies that come within the acquire radius are chased, as long as they stay within the leash of the station point.
static void army_order_station_decide(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order& order = armies.orders[army_index];

	if (order_engaged_is_valid(world, army_index))
	{
		world_location engagedLocation = world_entity_get_location(world, order.engaged);
		if (vec2_dist_squared(order.location, engagedLocation) <= ORDER_STATION_LEASH * ORDER_STATION_LEASH)
		{
			armies.movements[army_index].destination = engagedLocation;
			return;
		}
	}
	order.engaged = INVALID_ENTITY_GUID;

	armies.movements[army_index].destination = order.location;

	entity_guid found = order_find_hostile_army(world, army_index, armies.locations[army_index], ORDER_ACQUIRE_RADIUS, 0xFFFF);
	if (found.is_valid() && vec2_dist_squared(order.location, world_entity_get_location(world, found)) <= ORDER_STATION_LEASH * ORDER_STATION_LEASH)
	{
		order.engaged = found;
		armies.movements[army_index].destination = world_entity_get_location(world, found);
	}
	// @TODO(Marc): Caravans (hostile or neutral) within the acquire radius should be attacked too. There is no caravan attack resolution yet.
}

// ATTACK

// Chase the target's current location. The order ends (STATION) if the target is gone. A settlement that is not hostile anymore is defended instead.
static void army_order_attack_decide(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid target = armies.orders[army_index].target;

	if (!world_entity_is_valid(world, target))
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	if (target.get_type() == ENTITY_TYPE::SETTLEMENT && world.entities->settlements.owners[target.get_index()] == armies.owners[army_index])
	{
		army_order_defend_territory(armies, army_index, target, armies.locations[army_index]);
		return; // Worked on from the next tick.
	}

	armies.movements[army_index].destination = world_entity_get_location(world, target);
}

// Close enough to the target, the attack is resolved (a siege or engagement starts or is joined, which ends the chase).
static void army_order_attack_arrive(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid target = armies.orders[army_index].target;

	// An earlier resolution this tick may have destroyed the target: handled by the decision next tick.
	if (!world_entity_is_valid(world, target)) return;

	if (vec2_dist_squared(armies.locations[army_index], world_entity_get_location(world, target)) > ATTACK_REACH_TILES * ATTACK_REACH_TILES) return;

	army_order_station(armies, army_index, armies.locations[army_index]);
	world_movement_stop(armies.movements[army_index], armies.locations[army_index]);
	world_resolve_attack(world, armies.guids[army_index], target);
}

// DEFEND_TERRITORY (target = an own settlement)

// Patrol: walk between random points of the territory, attack hostile armies inside it. If the hostile armies in or near the territory outnumber the army, it retreats
// to the settlement tile and stands there, fighting what comes within the acquire radius, until none is left near.
static void army_order_defend_decide(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order& order = armies.orders[army_index];
	entity_guid settlement = order.target;

	// The settlement is gone or lost: nothing to defend.
	if (!world_entity_is_valid(world, settlement) || settlement.get_type() != ENTITY_TYPE::SETTLEMENT
		|| world.entities->settlements.owners[settlement.get_index()] != armies.owners[army_index])
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	ui16 settlementIndex = settlement.get_index();
	world_location settlementLocation = world.entities->settlements.locations[settlementIndex];
	ui64 threat = order_hostile_men(world, army_index, settlementLocation, ORDER_DEFEND_THREAT_RADIUS, settlementIndex);
	ui64 mine = army_total_manpower(armies.compositions[army_index]);

	if (order.state == ORDER_DEFEND_PATROL && threat > mine)
	{
		order.state = ORDER_DEFEND_RETREATED;
		order.engaged = INVALID_ENTITY_GUID;
	}
	else if (order.state == ORDER_DEFEND_RETREATED && threat == 0)
	{
		order.state = ORDER_DEFEND_PATROL;
		order.engaged = INVALID_ENTITY_GUID;
	}

	if (order.state == ORDER_DEFEND_RETREATED)
	{
		// Stand on the settlement tile, fight what comes close.
		order.location = settlementLocation;
		if (order_engaged_is_valid(world, army_index))
		{
			armies.movements[army_index].destination = world_entity_get_location(world, order.engaged);
			return;
		}
		order.engaged = order_find_hostile_army(world, army_index, armies.locations[army_index], ORDER_ACQUIRE_RADIUS, 0xFFFF);
		armies.movements[army_index].destination = order.engaged.is_valid() ? world_entity_get_location(world, order.engaged) : settlementLocation;
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

	// No intruder: wander. A new point is picked at random while standing on the current one, or at once if it is not a point of the territory.
	bool onPoint = armies.locations[army_index] == order.location;
	bool pointValid = order_tile_owned_by(world, order.location, settlementIndex);
	if (!pointValid || (onPoint && world.rand_gen.next<ui32>() % ORDER_DEFEND_WANDER_CHANCE == 0))
		order.location = order_pick_point(world, settlementIndex, settlementLocation, ORDER_DEFEND_WANDER_RADIUS);
	armies.movements[army_index].destination = order.location;
}

// RAID (target = a hostile settlement)

// Go to a point inside the target's territory and stay there. Flees stronger hostile armies close by (towards the home settlement) and comes back when none is near.
static void army_order_raid_decide(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order& order = armies.orders[army_index];
	entity_guid target = order.target;

	// The target is gone or not hostile anymore.
	if (!world_entity_is_valid(world, target) || target.get_type() != ENTITY_TYPE::SETTLEMENT
		|| world.entities->settlements.owners[target.get_index()] == armies.owners[army_index])
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	ui16 targetIndex = target.get_index();
	world_location targetLocation = world.entities->settlements.locations[targetIndex];
	ui64 mine = army_total_manpower(armies.compositions[army_index]);

	if (order.state == ORDER_RAID_RAIDING && order_hostile_men(world, army_index, armies.locations[army_index], ORDER_RAID_FLEE_RADIUS, 0xFFFF) > mine)
		order.state = ORDER_RAID_FLEEING;
	else if (order.state == ORDER_RAID_FLEEING && order_hostile_men(world, army_index, armies.locations[army_index], ORDER_RAID_RETURN_RADIUS, 0xFFFF) == 0)
		order.state = ORDER_RAID_RAIDING;

	if (order.state == ORDER_RAID_FLEEING)
	{
		// Run to the home settlement. Without one, stand and take what comes.
		entity_guid home = armies.home_settlements[army_index];
		if (world_entity_is_valid(world, home) && home.get_type() == ENTITY_TYPE::SETTLEMENT && world.entities->settlements.owners[home.get_index()] == armies.owners[army_index])
			armies.movements[army_index].destination = world.entities->settlements.locations[home.get_index()];
		else
			army_order_fall_back_to_station(world, army_index);
		return;
	}

	// Raiding: stand on a point of the territory.
	if (!order_tile_owned_by(world, order.location, targetIndex))
		order.location = order_pick_point(world, targetIndex, targetLocation, ORDER_RAID_POINT_RADIUS);
	armies.movements[army_index].destination = order.location;

	if (armies.locations[army_index] == order.location)
	{
		// @TODO(Marc): P3c: raid loot hook. The army standing in the target's territory takes its share of the settlement's production here.
		// @TODO(Marc): Enemy / neutral caravans close by should be attacked too. There is no caravan attack resolution yet.
	}
}

// GO_HOME

// Walk to the home settlement. Without a home settlement of the owner, the army stands where it is.
static void army_order_go_home_decide(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid home = armies.home_settlements[army_index];

	if (!world_entity_is_valid(world, home) || home.get_type() != ENTITY_TYPE::SETTLEMENT || world.entities->settlements.owners[home.get_index()] != armies.owners[army_index])
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}
	armies.movements[army_index].destination = world.entities->settlements.locations[home.get_index()];
}

// Close enough to the home settlement (and inside its territory, like the disband command requires): the army disbands into it.
static void army_order_go_home_arrive(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid home = armies.home_settlements[army_index];
	if (!world_entity_is_valid(world, home) || home.get_type() != ENTITY_TYPE::SETTLEMENT || world.entities->settlements.owners[home.get_index()] != armies.owners[army_index]) return;

	ui16 homeIndex = home.get_index();
	if (vec2_dist_squared(armies.locations[army_index], world.entities->settlements.locations[homeIndex]) > ORDER_GO_HOME_DISBAND_TILES * ORDER_GO_HOME_DISBAND_TILES) return;
	if (!order_tile_owned_by(world, armies.locations[army_index], homeIndex)) return;

	world_disband_army(world, armies.guids[army_index], home);
}

// One row per ARMY_ORDER_TYPE, in order.
static const army_order_def ARMY_ORDER_DEFS[ARMY_ORDER_TYPE_COUNT] =
{
	{ "STATION", army_order_station_decide, army_order_engaged_arrive },
	{ "ATTACK", army_order_attack_decide, army_order_attack_arrive },
	{ "DEFEND_TERRITORY", army_order_defend_decide, army_order_engaged_arrive },
	{ "RAID", army_order_raid_decide, nullptr },
	{ "GO_HOME", army_order_go_home_decide, army_order_go_home_arrive },
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
		if (def == nullptr || def->decide == nullptr)
		{
			army_order_fall_back_to_station(world, (ui16)armyIndex);
			continue;
		}
		def->decide(world, (ui16)armyIndex);
	}
}

void world_system_army_orders_arrival(match_world_state& world)
{
	world_entity_armies& armies = world.entities->armies;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;
		if (armies.engagements[armyIndex].is_valid()) continue; // Already fighting.
		if (armies.raising[armyIndex]) continue;

		const army_order_def* def = armies.orders[armyIndex].type < ARMY_ORDER_TYPE_COUNT ? &ARMY_ORDER_DEFS[armies.orders[armyIndex].type] : nullptr;
		if (def != nullptr && def->arrive != nullptr) def->arrive(world, (ui16)armyIndex);
	}
}
