#include "world_sim_entity_army.h"
#include "../world_sim.h"
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
	army_troop_count men = world.entities->armies.compositions[army.get_index()];

	// Levies return as unemployed population (settlement_release_raised), the other types rejoin the local troops (home).
	for (ui8 troopType = 0; troopType < TROOP_TYPE_COUNT; troopType++)
	{
		settlement_release_raised(world, settlement.get_index(), troopType, troop_count_field(men, troopType));
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

void world_army_take_casualties(match_world_state& world, entity_guid army, const ui32 kills[world_entity_engagements::UNIT_TYPES], ui32 out_dead[world_entity_engagements::UNIT_TYPES])
{
	world_entity_armies& armies = world.entities->armies;

	for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++)
	{
		out_dead[unitType] = army_remove_troops(world, army.get_index(), unitType, (ui16)ia_min(kills[unitType], (ui32)0xFFFF));
	}

	// The dead also leave their home settlement (raised counts and total population). Skipped if the home is gone.
	entity_guid home = armies.home_settlements[army.get_index()];
	if (!home.is_valid() || home.get_type() != ENTITY_TYPE::SETTLEMENT || !world_entity_is_valid(world, home)
            || world.entities->settlements.owners[home.get_index()] != armies.owners[army.get_index()]) return;

	for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++)
	{
		settlement_remove_soldiers_raised(world, home.get_index(), unitType, (ui16)out_dead[unitType]);
	}
}

ui16 world_army_desertion(match_world_state& world, entity_guid army, ui8 troop_type, ui16 count)
{
	world_entity_armies& armies = world.entities->armies;
	ui16 deserters = army_remove_troops(world, army.get_index(), troop_type, count);
	if (deserters == 0) return 0;

	// An army in a engagement no longer counts them among its side's men.
	entity_guid engagement = armies.engagements[army.get_index()];
	if (engagement.is_valid())
	{
		ui32& sideMen = world.entities->engagements.states[engagement.get_index()].side(armies.engagement_sides[army.get_index()]).men[troop_type];
		sideMen -= ia_min(sideMen, (ui32)deserters);
	}

	// They only count at home if the home settlement still is the owner's. Otherwise they are gone (like casualties).
	entity_guid home = armies.home_settlements[army.get_index()];
	if (home.is_valid() && home.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, home)
		&& world.entities->settlements.owners[home.get_index()] == armies.owners[army.get_index()])
	{
		// Those the settlement cannot take back (full pool) are lost.
		ui16 returned = settlement_release_raised(world, home.get_index(), troop_type, deserters);
		settlement_remove_soldiers_raised(world, home.get_index(), troop_type, deserters - returned);
	}

	// The last man leaving ends the army. The army is invalid after this.
	if (army_total_manpower(armies.compositions[army.get_index()]) == 0) world_destroy_army(world, army);
	return deserters;
}

bool army_can_exit_current_engagement(const match_world_state& world, ui16 army_index)
{
	const world_entity_armies& armies = world.entities->armies;
	entity_guid engagement = armies.engagements[army_index];
	if (!engagement.is_valid()) return false;

	// Only an idle siege has no fighting: its armies just walk out of it.
	return world.entities->engagements.states[engagement.get_index()].phase == world_entity_engagements::PHASE_BESIEGING;
}

void army_exit_engagement(match_world_state& world, ui16 army_index)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid engagement = armies.engagements[army_index];
	if (!engagement.is_valid()) return;

	// Its men leave their side (a siege recounts its sides every round anyway).
	world_entity_engagements::side_state& side = world.entities->engagements.states[engagement.get_index()].side(armies.engagement_sides[army_index]);
	ui32 counts[world_entity_engagements::UNIT_TYPES];
	army_unit_counts(armies.compositions[army_index], counts);
	for (ui8 unitType = 0; unitType < world_entity_engagements::UNIT_TYPES; unitType++) side.men[unitType] -= ia_min(counts[unitType], side.men[unitType]);

	armies.engagements[army_index] = INVALID_ENTITY_GUID;
	world_engagement_army_left(world, engagement); // May end a siege left without attackers.
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

	armies.engagement_sides = memory.alloc<ui8>(count);
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
