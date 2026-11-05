// Standing orders of armies: each tick, what an army moves towards and what it does on arrival, from its engagement or its order (ARMY_ORDER_TYPE).
// An army in an engagement it cannot leave (or whose order wants it there) walks to the engagement's location (the order is not worked on meanwhile and goes on afterwards):
// every order logic starts with its own handling of the army's engagement. Raising armies ignore their order until raising ends (except for that part).
// The order logic of each type is a row of ARMY_ORDER_DEFS.
// Hostility is world_entity_is_hostile. Targets are found by scanning all armies (no spatial index yet). Tunables are in GAME_PARAMETERS (order_*).

#include "game_common/match/world/world_state.h"

#include "systems_war.h"
#include "../world_sim.h"

#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_engagement.h"
#include "../entities/world_sim_entity_settlement.h"

#include "../world_movement.h"

// Order logic: one function per order type, run once per tick before movement, for armies that are not raising or pinned in an engagement.
// It handles arrival first (the army is close enough to what it was going for, from its position after the last movement), then sets the movement
// destination (world_army_travel_to) and replaces the order if it became invalid. Gets the army's slot index.
// Each logic starts with its own handling of the army's engagement: an army in an engagement it cannot leave (world_army_can_leave_current_engagement), or that the order
// wants it to stay in, is pinned there (engagement_battle_logic) and the logic returns; otherwise it leaves and the logic goes on (a raising army waits for the next tick).
struct army_order_def
{
	const char* name;
	void (*logic)(match_world_state& world, ui16 army_index);
};

// HELPERS

// What an army in an engagement does while its order does not overwrite it: for now walks to the engagement's location, where it is pinned once close enough.
// The order is not worked on meanwhile and goes on afterwards. @TODO(Marc): Later the default behavior for engagements an order does not take over.
static void engagement_battle_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	world_location engagementLocation = world.entities->engagements.location[armies.engagement[army_index].get_index()];
	armies.movement[army_index].destination = vec2_dist_squared(armies.location[army_index], engagementLocation) > GAME_PARAMETERS.engagement_pin_distance_tiles * GAME_PARAMETERS.engagement_pin_distance_tiles
		? engagementLocation : armies.location[army_index];
}

// An order that cannot go on: the army stands where it is.
static void army_order_fall_back_to_station(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order_station(armies, army_index, armies.location[army_index]);
	world_movement_stop(armies.movement[army_index], armies.location[army_index]);
}

// Whether the entity (any type, may be invalid) is hostile to the army's owner and still worth targeting: armies must have men.
static bool order_is_hostile_entity(const match_world_state& world, ui16 army_index, entity_guid entity)
{
	if (!world_entity_is_valid(world, entity) || !world_entity_is_hostile(world, entity, world.entities->armies.owner[army_index])) return false;
	if (entity.get_type() == ENTITY_TYPE::ARMY) return army_total_manpower(world.entities->armies.composition[entity.get_index()]) > 0;
	return true;
}

// The hostile army nearest to the center within the radius (lowest slot on a tie), or an invalid guid. With territory_of set, only armies inside that settlement's territory.
static entity_guid order_find_hostile_army(match_world_state& world, ui16 army_index, world_location center, float radius, ui16 territory_of)
{
	const world_entity_armies& armies = world.entities->armies;
	entity_guid found = INVALID_ENTITY_GUID;
	float foundDist = 0;
	for (ui32 other = 0; other < armies.max_count; other++)
	{
		if (!order_is_hostile_entity(world, army_index, armies.guids[other])) continue;
		// An army in an engagement stands for it: not chased (nor joined) if the engagement is neutral to this army's owner. Otherwise the chase ends in the join.
		if (armies.engagement[other].is_valid() && world_entity_is_neutral(world, armies.engagement[other], armies.owner[army_index])) continue;

		float dist = vec2_dist_squared(center, armies.location[other]);
		if (dist > radius * radius) continue;
		if (found.is_valid() && dist >= foundDist) continue;
		if (territory_of != 0xFFFF && !world_tile_owned_by_settlement(world, armies.location[other], territory_of)) continue;

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
		if (!order_is_hostile_entity(world, army_index, armies.guids[other])) continue;

		bool near = vec2_dist_squared(center, armies.location[other]) <= radius * radius;
		bool inside = territory_of != 0xFFFF && world_tile_owned_by_settlement(world, armies.location[other], territory_of);
		if (near || inside) men += army_total_manpower(armies.composition[other]);
	}
	return men;
}

// Sets the army's destination to the location. Returns whether it has arrived, meaning it is within the given tiles of it (from its position after the last movement).
// Only for order logic: the army's current order sets its destination every tick, so it would override anything else setting it.
static bool world_army_travel_to(match_world_state& world, ui16 army_index, world_location location, ui32 arrive_tiles)
{
	world_entity_armies& armies = world.entities->armies;
	armies.movement[army_index].destination = location;
	return world_location_within_tiles(armies.location[army_index], location, arrive_tiles);
}

// STATION

// Go to the order's location. Hostile armies that come within the acquire radius are chased, as long as they stay within the leash of the station point.
static void army_order_station_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	world_entity_engagements& engagements = world.entities->engagements;
	army_order& order = armies.order[army_index];

	// An army stationed within the join radius of an engagement joins it and stays in it. Engagements that do not aggro the army (idle sieges it attacks) are joined here.
	if (armies.engagement[army_index].is_valid())
	{
		entity_guid engagement = armies.engagement[army_index];
		bool stationedInside = world_location_within_tiles(order.location, engagements.location[engagement.get_index()], engagement_join_radius(world, engagement));
		if (stationedInside || !world_army_can_leave_current_engagement(world, armies.guids[army_index]))
		{
			engagement_battle_logic(world, army_index);
			return;
		}
		world_army_leave_current_engagement(world, armies.guids[army_index]);
		if (armies.raising[army_index]) return;
	}
	else if (army_total_manpower(armies.composition[army_index]) > 0)
	{
		for (ui32 engagementIndex = 0; engagementIndex < engagements.max_count; engagementIndex++)
		{
			entity_guid engagement = engagements.guids[engagementIndex];
			if (!engagement.is_valid()) continue;

			ui32 radius = engagement_join_radius(world, engagement);
			if (!world_location_within_tiles(order.location, engagements.location[engagementIndex], radius)
				|| !world_location_within_tiles(armies.location[army_index], engagements.location[engagementIndex], radius)) continue;

			if (world_army_join_engagement(world, armies.guids[army_index], engagement))
			{
				engagement_battle_logic(world, army_index);
				return;
			}
		}
	}

	// Arrival: close enough to the chased army, the attack is resolved (an engagement starts or is joined, which ends the chase).
	if (order_is_hostile_entity(world, army_index, order.engaged)
		&& world_location_within_tiles(armies.location[army_index], world_entity_get_location(world, order.engaged), GAME_PARAMETERS.order_attack_reach_tiles))
	{
		entity_guid target = order.engaged;
		order.engaged = INVALID_ENTITY_GUID;
		world_movement_stop(armies.movement[army_index], armies.location[army_index]);
		world_army_attack_entity(world, armies.guids[army_index], target);
		return;
	}

	if (order_is_hostile_entity(world, army_index, order.engaged))
	{
		world_location engagedLocation = world_entity_get_location(world, order.engaged);
		if (world_location_within_tiles(order.location, engagedLocation, GAME_PARAMETERS.order_station_leash))
		{
			armies.movement[army_index].destination = engagedLocation;
			return;
		}
	}
	order.engaged = INVALID_ENTITY_GUID;

	world_army_travel_to(world, army_index, order.location, 0); // Arriving changes nothing: it keeps watching for hostile armies.

	entity_guid found = order_find_hostile_army(world, army_index, armies.location[army_index], (float)GAME_PARAMETERS.order_station_acquire_radius, 0xFFFF);
	if (found.is_valid() && world_location_within_tiles(order.location, world_entity_get_location(world, found), GAME_PARAMETERS.order_station_leash))
	{
		order.engaged = found;
		armies.movement[army_index].destination = world_entity_get_location(world, found);
	}
	// @TODO(Marc): Caravans (hostile or neutral) within the acquire radius should be attacked too. There is no caravan attack resolution yet.
}

// ATTACK

// Walks to the point at distance radius + 1 tiles of the center, on the line from the center through the army: just outside the join radius.
static void order_wait_outside(match_world_state& world, ui16 army_index, world_location center, ui32 radius)
{
	world_entity_armies& armies = world.entities->armies;
	const world_dimensions& size = world.terrain->size_tiles;

	i32 dx = (i32)armies.location[army_index].x - (i32)center.x;
	i32 dy = (i32)armies.location[army_index].y - (i32)center.y;
	i32 dist = (i32)ia_sqrt_i((ui64)(dx * dx + dy * dy));
	i32 edge = (i32)radius + 1;
	i32 x = dist == 0 ? (i32)center.x + edge : (i32)center.x + dx * edge / dist;
	i32 y = dist == 0 ? (i32)center.y : (i32)center.y + dy * edge / dist;
	x = ia_clamped(x, (i32)0, (i32)size.x - 1);
	y = ia_clamped(y, (i32)0, (i32)size.y - 1);
	armies.movement[army_index].destination = { (ui16)x, (ui16)y };
}

// Chase the target's current location. Close enough to it, the attack is resolved (a siege or engagement starts or is joined, which pins the army: the order goes on afterwards).
// The target stands for an engagement if it is one, is in one, or is a besieged settlement: the army goes to it (order location = the engagement's) and joins it if it is
// joinable (resolve, or the proximity join); if it is neutral to the owner the army waits just outside its join radius, order kept.
// The order ends (STATION) if the target is gone. A settlement that is not hostile anymore is defended instead.
static void army_order_attack_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid target = armies.order[army_index].target;

	// The engagement the target stands for: the target itself, the engagement a target army is in, or the siege of a target settlement.
	entity_guid engagement = INVALID_ENTITY_GUID;
	if (world_entity_is_valid(world, target))
	{
		switch (target.get_type())
		{
		case ENTITY_TYPE::ENGAGEMENT: engagement = target; break;
		case ENTITY_TYPE::ARMY: engagement = armies.engagement[target.get_index()]; break;
		case ENTITY_TYPE::SETTLEMENT: engagement = world.entities->settlements.siege[target.get_index()]; break;
		default: break;
		}
	}

	// An army in that engagement stays in it. Any other engagement it can leave, it leaves.
	if (armies.engagement[army_index].is_valid())
	{
		if (armies.engagement[army_index] == engagement || !world_army_can_leave_current_engagement(world, armies.guids[army_index]))
		{
			engagement_battle_logic(world, army_index);
			return;
		}
		world_army_leave_current_engagement(world, armies.guids[army_index]);
		if (armies.raising[army_index]) return;
	}

	if (!world_entity_is_valid(world, target))
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	if (target.get_type() == ENTITY_TYPE::SETTLEMENT && !world_entity_is_hostile(world, target, armies.owner[army_index]))
	{
		army_order_defend_territory(armies, army_index, target, armies.location[army_index]);
		return; // Worked on from the next tick.
	}

	if (world_entity_is_valid(world, engagement))
	{
		world_location engagementLocation = world.entities->engagements.location[engagement.get_index()];
		armies.order[army_index].location = engagementLocation;
		if (world_entity_is_neutral(world, engagement, armies.owner[army_index]))
		{
			order_wait_outside(world, army_index, engagementLocation, engagement_join_radius(world, engagement));
			return;
		}
		if (world_army_travel_to(world, army_index, engagementLocation, GAME_PARAMETERS.order_attack_reach_tiles))
		{
			world_movement_stop(armies.movement[army_index], armies.location[army_index]);
			world_army_attack_entity(world, armies.guids[army_index], engagement);
		}
		return;
	}

	// Arriving resolves the attack (which engages the army, or conquers an undefended settlement). The order stays.
	if (world_army_travel_to(world, army_index, world_entity_get_location(world, target), GAME_PARAMETERS.order_attack_reach_tiles))
	{
		world_movement_stop(armies.movement[army_index], armies.location[army_index]);
		world_army_attack_entity(world, armies.guids[army_index], target);
	}
}

// DEFEND_TERRITORY (target = an own settlement)

// Patrol: stand on the settlement (no wandering for now), attack hostile armies inside the territory. If the hostile armies in or near the territory outnumber the army, it retreats
// to the settlement tile and stands there, fighting what comes within the acquire radius, until none is left near.
static void army_order_defend_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order& order = armies.order[army_index];
	entity_guid settlement = order.target;

	// An army in the siege of the settlement it defends stays in it. Any other engagement it can leave, it leaves.
	if (armies.engagement[army_index].is_valid())
	{
		const world_entity_engagements& engagements = world.entities->engagements;
		ui16 engagementIndex = armies.engagement[army_index].get_index();
		if ((engagements.kind[engagementIndex] == ENGAGEMENT_KIND::SIEGE && engagements.main_target[engagementIndex] == settlement) || !world_army_can_leave_current_engagement(world, armies.guids[army_index]))
		{
			engagement_battle_logic(world, army_index);
			return;
		}
		world_army_leave_current_engagement(world, armies.guids[army_index]);
		if (armies.raising[army_index]) return;
	}

	// Arrival: close enough to the chased army, the attack is resolved (an engagement starts or is joined, which ends the chase).
	if (order_is_hostile_entity(world, army_index, order.engaged)
		&& world_location_within_tiles(armies.location[army_index], world_entity_get_location(world, order.engaged), GAME_PARAMETERS.order_attack_reach_tiles))
	{
		entity_guid target = order.engaged;
		order.engaged = INVALID_ENTITY_GUID;
		world_movement_stop(armies.movement[army_index], armies.location[army_index]);
		world_army_attack_entity(world, armies.guids[army_index], target);
		return;
	}

	// The settlement is gone or lost: nothing to defend.
	if (!world_entity_is_valid(world, settlement) || settlement.get_type() != ENTITY_TYPE::SETTLEMENT
		|| world_entity_is_hostile(world, settlement, armies.owner[army_index]))
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	ui16 settlementIndex = settlement.get_index();
	world_location settlementLocation = world.entities->settlements.location[settlementIndex];
	ui64 threat = order_hostile_men(world, army_index, settlementLocation, (float)GAME_PARAMETERS.order_defend_threat_radius, settlementIndex);
	ui64 mine = army_total_manpower(armies.composition[army_index]);

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

	// The objective area is the territory: travelling while outside it, patrolling inside it.
	if (order.state != ARMY_ORDER_STATE_DEFEND_RETREATED)
		order.state = world_tile_owned_by_settlement(world, armies.location[army_index], settlementIndex) ? ARMY_ORDER_STATE_DEFEND_PATROL : ARMY_ORDER_STATE_TRAVELLING;

	if (order.state == ARMY_ORDER_STATE_DEFEND_RETREATED)
	{
		// Stand on the settlement tile, fight what comes close.
		order.location = settlementLocation;
		if (order_is_hostile_entity(world, army_index, order.engaged))
		{
			armies.movement[army_index].destination = world_entity_get_location(world, order.engaged);
			return;
		}
		order.engaged = order_find_hostile_army(world, army_index, armies.location[army_index], (float)GAME_PARAMETERS.order_station_acquire_radius, 0xFFFF);
		if (order.engaged.is_valid()) armies.movement[army_index].destination = world_entity_get_location(world, order.engaged);
		else world_army_travel_to(world, army_index, settlementLocation, 0);
		return;
	}

	// Patrol: intruders first (only while they are inside the territory).
	if (order_is_hostile_entity(world, army_index, order.engaged) && world_tile_owned_by_settlement(world, world_entity_get_location(world, order.engaged), settlementIndex))
	{
		armies.movement[army_index].destination = world_entity_get_location(world, order.engaged);
		return;
	}
	order.engaged = order_find_hostile_army(world, army_index, armies.location[army_index], 1e9f, settlementIndex);
	if (order.engaged.is_valid())
	{
		armies.movement[army_index].destination = world_entity_get_location(world, order.engaged);
		return;
	}

	// No intruder: stand on the settlement.
	// @TODO(Marc): Patrol the territory (random points, see world_pick_random_point) instead of standing still.
	world_army_travel_to(world, army_index, settlementLocation, 0);
}

// RAID (target = a hostile settlement)

// Go to the target's territory, then raid it standing inside. Flees stronger hostile armies close by (towards the home settlement) and comes back when none is near.
static void army_order_raid_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	army_order& order = armies.order[army_index];
	entity_guid target = order.target;

	// Raiding wants no engagement: an army that can leave the one it is in leaves it.
	if (armies.engagement[army_index].is_valid())
	{
		if (!world_army_can_leave_current_engagement(world, armies.guids[army_index]))
		{
			engagement_battle_logic(world, army_index);
			return;
		}
		world_army_leave_current_engagement(world, armies.guids[army_index]);
		if (armies.raising[army_index]) return;
	}

	// The target is gone or not hostile anymore.
	if (!world_entity_is_valid(world, target) || target.get_type() != ENTITY_TYPE::SETTLEMENT
		|| !world_entity_is_hostile(world, target, armies.owner[army_index]))
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	ui16 targetIndex = target.get_index();
	world_location targetLocation = world.entities->settlements.location[targetIndex];
	ui64 mine = army_total_manpower(armies.composition[army_index]);

	// Flees in every state but FLEEING.
	if (order.state != ARMY_ORDER_STATE_RAID_FLEEING
		&& order_hostile_men(world, army_index, armies.location[army_index], (float)GAME_PARAMETERS.order_raid_flee_radius, 0xFFFF) > mine)
		order.state = ARMY_ORDER_STATE_RAID_FLEEING;
	else if (order.state == ARMY_ORDER_STATE_RAID_FLEEING
		&& order_hostile_men(world, army_index, armies.location[army_index], (float)GAME_PARAMETERS.order_raid_return_radius, 0xFFFF) == 0)
		order.state = ARMY_ORDER_STATE_TRAVELLING; // Resolved below: walks back in, or resumes raiding if inside the territory.

	if (order.state == ARMY_ORDER_STATE_RAID_FLEEING)
	{
		// Run to the home settlement. Without one, stand and take what comes.
		entity_guid home = armies.home_settlement[army_index];
		if (world_entity_is_valid(world, home) && home.get_type() == ENTITY_TYPE::SETTLEMENT && world.entities->settlements.owner[home.get_index()] == armies.owner[army_index])
			world_army_travel_to(world, army_index, world.entities->settlements.location[home.get_index()], 0); // Keeps running until the threat is gone.
		else
			army_order_fall_back_to_station(world, army_index);
		return;
	}

	// Outside the territory: travel to the settlement until inside it. Inside: raiding starts, standing where the army entered.
	if (!world_tile_owned_by_settlement(world, armies.location[army_index], targetIndex))
	{
		order.state = ARMY_ORDER_STATE_TRAVELLING;
		world_army_travel_to(world, army_index, targetLocation, 0);
		return;
	}
	order.state = ARMY_ORDER_STATE_RAID_RAIDING;
	world_army_travel_to(world, army_index, armies.location[army_index], 0); // Stand.

	// Looting is world_system_raid_loot (daily, see world_sim_entity_army.cpp).
	// @TODO(Marc): Enemy / neutral caravans close by should be attacked too. There is no caravan attack resolution yet.
}

// GO_HOME

// Walk to the home settlement. Close enough to it (and inside its territory, like the disband command requires), the army disbands into it.
static void army_order_go_home_logic(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid home = armies.home_settlement[army_index];

	// Going home wants no engagement: an army that can leave the one it is in leaves it.
	if (armies.engagement[army_index].is_valid())
	{
		if (!world_army_can_leave_current_engagement(world, armies.guids[army_index]))
		{
			engagement_battle_logic(world, army_index);
			return;
		}
		world_army_leave_current_engagement(world, armies.guids[army_index]);
		if (armies.raising[army_index]) return;
	}

    bool originIsHostile = world_entity_is_hostile(world, home, armies.owner[army_index]);
	if (!world_entity_is_valid(world, home) || home.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		army_order_fall_back_to_station(world, army_index);
		return;
	}

	ui16 homeIndex = home.get_index();
	world_location homeLocation = world.entities->settlements.location[homeIndex];
	// Always travelling: arriving ends the order.
	if (world_army_travel_to(world, army_index, homeLocation, GAME_PARAMETERS.order_go_home_disband_tiles)
		&& world_tile_owned_by_settlement(world, armies.location[army_index], homeIndex))
	{
		if (!originIsHostile)
		{
			world_disband_army(world, armies.guids[army_index], home);
			return;
		}

		// The origin changed hands: the men give up and go back to it (it is the new owner's now). The last man leaving sends its gold to the treasury and its supplies to the home (only if still owned).
		entity_guid army = armies.guids[army_index];
		for (ui8 troopTypeIndex = 0; troopTypeIndex < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT && world_entity_is_valid(world, army); troopTypeIndex++)
		{
			world_army_desertion(world, army, (TROOP_TYPE)troopTypeIndex, armies.composition[army_index].count[troopTypeIndex]);
		}
	}
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

		// Raising armies stay where they were raised: their order (set by the raise command) starts when raising ends. Unless engaged: the order logic handles that.
		if (armies.raising[armyIndex] && !armies.engagement[armyIndex].is_valid()) continue;

		const army_order_def* def = armies.order[armyIndex].type < ARMY_ORDER_TYPE_COUNT ? &ARMY_ORDER_DEFS[armies.order[armyIndex].type] : nullptr;
		if (def == nullptr || def->logic == nullptr)
		{
			army_order_fall_back_to_station(world, (ui16)armyIndex);
			continue;
		}
		def->logic(world, (ui16)armyIndex);
	}
}
