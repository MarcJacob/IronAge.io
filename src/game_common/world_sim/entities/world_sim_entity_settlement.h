// Economy symbols shared between world simulation themes.

#ifndef WORLD_SIM_ECONOMY_INCLUDED
#define WORLD_SIM_ECONOMY_INCLUDED

#include "core/std_types.h"
#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world_state.h"

#include "world_sim_entity_army.h"

// Spawns a new settlement in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_settlement(match_world_state& world, const world_entity_settlements::single& start_state);
void world_destroy_settlement(match_world_state& world, entity_guid settlement);

// Allocates the settlements' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_settlements(match_world_state& world, mem_arena& memory, ui32 count);

// Gives the settlement to a new owner. Its tax rate resets to 0.
void world_settlement_change_owner(match_world_state& world, ui16 settlement_index, match_player_id new_owner);

// SETTLEMENT POPULATION PRIMITIVES
// Every change to a settlement's peasants / home / raised counts and population_total goes through these (taking the settlement's index).
// They keep population_total = peasants + home + raised, never underflow (they return what was actually moved) and do not touch the player stats.
// Troop types are the TROOP_TYPE_* of world_sim_army.h. Levies have no home pool (home.levies stays 0).

// Deaths. Removes up to count peasants, returns how many were removed.
static inline ui32 settlement_remove_peasants(match_world_state& world, ui16 settlement_index, ui32 count)
{
	auto& peasants = world.entities->settlements.pop_peasants[settlement_index];
	ui32 removed = ia_min(count, peasants);
	peasants -= removed;
	world.entities->settlements.pop_total[settlement_index] -= removed;

	return removed;
}

// COLONISTS' GOODS (separate block)
// Goods the given number of colonists would take from the settlement's Market Goods: pool x leaving / (leaving + staying), staying = the whole remaining population (home or not).
// Call before the colonists are removed (pop_total is then leaving + staying). Only the goods pool is shared out (there is no local wealth).
static inline ui32 settlement_colonist_goods(const match_world_state& world, ui16 settlement_index, ui32 colonists)
{
	ui32 total = world.entities->settlements.pop_total[settlement_index];
	if (total == 0) return 0;

	ui64 goods = world.entities->settlements.trade[settlement_index].market_goods;
	return (ui32)(goods * ia_min(colonists, total) / total);
}

// Deaths of raised soldiers. Removes up to count raised soldiers of a type, returns how many were removed.
static inline ui16 settlement_remove_soldiers_raised(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 count)
{
	auto& raisedSoldiers = world.entities->settlements.raised[settlement_index];
	ui16& raised = troop_count_field(raisedSoldiers, troop_type);
	ui16 removed = ia_min(count, raised);
	raised -= removed;
	world.entities->settlements.pop_total[settlement_index] -= ia_min((ui32)removed, world.entities->settlements.pop_total[settlement_index]);

	return removed;
}

// Training. Moves up to count peasants into the garrison of a type. Returns how many were moved.
static inline ui16 settlement_train_soldiers(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui32 count)
{
	auto& garrisonTroops = world.entities->settlements.garrison[settlement_index];
	ui16& troops = troop_count_field(garrisonTroops, troop_type);

    ui16 moved = 0;

    auto& unemployed = world.entities->settlements.pop_unemployed[settlement_index];
    auto& peasants = world.entities->settlements.pop_peasants[settlement_index];

    ui16 fromUnemployed = ia_min(unemployed, count);
    unemployed -= fromUnemployed;
    moved += fromUnemployed;

    // If unemployed population was not sufficient, take from peasants.
    if (count > moved)
    {
        ui16 fromPeasants = ia_min(peasants, count - moved);
        peasants -= fromPeasants;
        moved += fromPeasants;
    }

	troops += (ui16)moved;
	return (ui16)moved;
}

// Discharge. Moves up to count garrison soldiers of a type back to peasants. Returns how many were moved.
static inline ui16 settlement_discharge_soldiers(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 count)
{
	ui16& troops = troop_count_field(world.entities->settlements.garrison[settlement_index], troop_type);
	ui16 moved = ia_min(count, troops);
	troops -= moved;
	world.entities->settlements.pop_unemployed[settlement_index] += moved;
	return moved;
}

// Raising from the garrison. Moves up to count home soldiers of a type to raised. Returns how many were moved.
static inline ui16 settlement_raise_soldiers(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 count)
{
	ui16& garrisonTroops = troop_count_field(world.entities->settlements.garrison[settlement_index], troop_type);
	ui16& raised = troop_count_field(world.entities->settlements.raised[settlement_index], troop_type);

	ui16 moved = ia_min(count, garrisonTroops);

	garrisonTroops -= moved;
	raised += moved;

	return moved;
}

// Standing down. Moves up to count raised soldiers of a type back to the settlement: they rejoin the garrison.
// Returns how many were moved.
static inline ui16 settlement_release_raised(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 count)
{
	ui16& garrisonTroops = troop_count_field(world.entities->settlements.garrison[settlement_index], troop_type);
	ui16& raised = troop_count_field(world.entities->settlements.raised[settlement_index], troop_type);

	ui16 moved = ia_min(count, raised);

	garrisonTroops += moved;
	raised -= moved;

	return moved;
}

// Construction manpower. Moves up to count free unemployed (then peasants) into workers. Returns how many were moved. population_total is unchanged.
static inline ui32 settlement_assign_workers(match_world_state& world, ui16 settlement_index, ui32 count)
{
    auto& workers = world.entities->settlements.pop_workers[settlement_index];

    ui16 moved = 0;

    auto& unemployed = world.entities->settlements.pop_unemployed[settlement_index];
    auto& peasants = world.entities->settlements.pop_peasants[settlement_index];

    ui16 fromUnemployed = ia_min(unemployed, count);
    unemployed -= fromUnemployed;
    moved += fromUnemployed;

    // If unemployed population was not sufficient, take from peasants.
    if (count > moved)
    {
        ui16 fromPeasants = ia_min(peasants, count - moved);
        peasants -= fromPeasants;
        moved += fromPeasants;
    }

	workers += moved;
	return moved;
}

// Construction manpower. Moves up to count workers back to free peasants. Returns how many were moved.
static inline ui32 settlement_release_workers(match_world_state& world, ui16 settlement_index, ui32 count)
{
    auto& workers = world.entities->settlements.pop_workers[settlement_index];

    ui16 moved = ia_min(count, workers);

    auto& unemployed = world.entities->settlements.pop_unemployed[settlement_index];

    unemployed += moved;
	workers -= moved;

	return moved;
}

// Deaths of workers. Removes up to count workers, returns how many were removed. Projects holding them are not told (they re-check their crew).
static inline ui32 settlement_remove_workers(match_world_state& world, ui16 settlement_index, ui32 count)
{
    auto& workers = world.entities->settlements.pop_workers[settlement_index];

    ui16 removed = ia_min(count, workers);
	workers -= removed;

    world.entities->settlements.pop_total[settlement_index] -= removed;

	return removed;
}

// Deaths of home soldiers (local pool). Removes up to count garrison soldiers of a type, returns how many were removed.
static inline ui16 settlement_remove_soldiers_home(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 count)
{
    auto& troops = troop_count_field(world.entities->settlements.garrison[settlement_index], troop_type);

    ui16 removed = ia_min(count, troops);
	troops -= removed;
	world.entities->settlements.pop_total[settlement_index] -= removed;

	return removed;
}

// MARKET GOODS
// Two pools: market goods (local, consumed) and export goods (caravans only). They share one capacity, which never refuses goods (excess is wasted daily).

// Capacity of both pools together = people at home + buildings' part.
static inline ui32 settlement_market_capacity(const world_entity_settlements& settlements, ui16 settlement_index)
{
	return settlements.at_home(settlement_index) * 4 + settlements.trade[settlement_index].market_capacity_buildings;
}

// Goods in both pools (local + export), the amount measured against the capacity.
static inline ui32 settlement_market_total(const world_entity_settlements& settlements, ui16 settlement_index)
{
	const settlement_trading& trade = settlements.trade[settlement_index];
	return (ui32)ia_min((ui64)trade.market_goods + trade.export_goods, (ui64)0xFFFFFFFF);
}

// Cap of the export pool = capacity x export ratio (clamped to 100) / 100, rounded down.
static inline ui32 settlement_export_cap(const world_entity_settlements& settlements, ui16 settlement_index)
{
	return (ui32)((ui64)settlement_market_capacity(settlements, settlement_index) * ia_min(settlements.trade[settlement_index].export_ratio, (ui8)100) / 100);
}

// Adds incoming goods, split by the export ratio (clamped to 100): export share = amount x ratio / 100 rounded down, kept only up to the room left under the export cap.
// The rest (rounding and export overflow included) goes to the local pool.
// Every goods income goes through here: production, caravan deliveries and refunds, colonists' goods.
static inline void settlement_market_add(world_entity_settlements& settlements, ui16 settlement_index, ui64 amount)
{
	settlement_trading& trade = settlements.trade[settlement_index];
	ui32 exportCap = settlement_export_cap(settlements, settlement_index);
	ui64 toExport = amount * ia_min(trade.export_ratio, (ui8)100) / 100;
	toExport = ia_min(toExport, (ui64)(exportCap > trade.export_goods ? exportCap - trade.export_goods : 0));
	ui64 toLocal = amount - toExport;
	ASSERT((ui64)trade.market_goods + toLocal <= 0xFFFFFFFF && (ui64)trade.export_goods + toExport <= 0xFFFFFFFF);
	trade.market_goods += (ui32)toLocal;
	trade.export_goods += (ui32)toExport;
}

// TRADE VOLUME

// Max effective trade volume = volume_max_multiplier x market capacity.
static inline ui32 settlement_trade_volume_max(const world_entity_settlements& settlements, ui16 settlement_index)
{
	ui64 max = (ui64)settlement_market_capacity(settlements, settlement_index) * GAME_PARAMETERS.trade.volume_max_multiplier;
	return (ui32)ia_min(max, (ui64)0xFFFFFFFF);
}

// Trade bonus fraction: min(volume, max) / max as a fixed-point fraction (0 to IA_FIXED_ONE). 0 if the max is 0.
static inline ui32 settlement_trade_volume_fraction(const world_entity_settlements& settlements, ui16 settlement_index)
{
	ui64 max = settlement_trade_volume_max(settlements, settlement_index);
	if (max == 0) return 0;
	ui64 effective = ia_min((ui64)settlements.trade[settlement_index].trade_volume, max);
	return (ui32)(effective * IA_FIXED_ONE / max);
}

// Adds a trade volume gain, up to the settlement's max: the excess is wasted. A volume already above the max is kept as it is.
static inline void settlement_trade_volume_add(world_entity_settlements& settlements, ui16 settlement_index, ui64 gain)
{
	ui32& volume = settlements.trade[settlement_index].trade_volume;
	ui64 max = settlement_trade_volume_max(settlements, settlement_index);
	if (volume >= max) return;
	volume = (ui32)ia_min((ui64)volume + gain, max);
}

// Claims a non-sea tile in the world for the given settlement (taking it from its current owner if any). Its travel distance comes from the settlement's neighbouring tiles.
// Returns the pressure cost (the tile's peasant job count).
// Ensure settlement's influence bounds rectangle is kept up to date !
ui16 world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index);

// SETTLEMENT BUILDING PRIMITIVES
// Every change to a settlement's building counts goes through these. They run the type's events (once per unit) and return how many units were actually added / removed.

static inline ui16 settlement_building_add(match_world_state& world, ui16 settlement_index, ui8 building_type_index, ui16 count)
{
	ASSERT(building_type_index < SETTLEMENT_BUILDING_TYPE_COUNT);
	world_entity_settlements& settlements = world.entities->settlements;
	ui16& units = settlements.buildings[settlement_index].building_counts[building_type_index];
	ui16 added = ia_min(count, (ui16)(0xFFFF - units));
	for (ui16 i = 0; i < added; i++)
	{
		units++;
		auto onAdded = SETTLEMENT_BUILDING_TABLE[building_type_index].events.on_added;
		if (onAdded) onAdded(world, settlements.guids[settlement_index]);
	}
	return added;
}

static inline ui16 settlement_building_remove(match_world_state& world, ui16 settlement_index, ui8 building_type_index, ui16 count)
{
	ASSERT(building_type_index < SETTLEMENT_BUILDING_TYPE_COUNT);
	world_entity_settlements& settlements = world.entities->settlements;
	ui16& units = settlements.buildings[settlement_index].building_counts[building_type_index];
	ui16 removed = ia_min(count, units);
	for (ui16 i = 0; i < removed; i++)
	{
		units--;
		auto onRemoved = SETTLEMENT_BUILDING_TABLE[building_type_index].events.on_removed;
		if (onRemoved) onRemoved(world, settlements.guids[settlement_index]);
	}
	return removed;
}

// Ceiling actually in force: the raw player-set ceiling capped by the settlement's troop max. It applies to the total of a type (home + raised).
// Levies have no max: their ceiling is the raw one.
static inline army_troop_count settlement_troop_effective_ceilings(const army_troop_count& troop_maxes, const army_troop_count& raw_ceilings)
{
	return {
		raw_ceilings.levies,
		raw_ceilings.men_at_arms < troop_maxes.men_at_arms ? raw_ceilings.men_at_arms : troop_maxes.men_at_arms,
		raw_ceilings.horsemen < troop_maxes.horsemen ? raw_ceilings.horsemen : troop_maxes.horsemen,
		raw_ceilings.knights < troop_maxes.knights ? raw_ceilings.knights : troop_maxes.knights,
	};
}

#endif // WORLD_SIM_ECONOMY_INCLUDED
