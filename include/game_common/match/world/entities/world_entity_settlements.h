#ifndef WORLD_ENTITY_SETTLEMENTS_INCLUDED
#define WORLD_ENTITY_SETTLEMENTS_INCLUDED

#include "core.h"
#include "world_entity_common.h"

// Optional (nullptr = nothing happens) type-specific behavior. Implemented in world_sim_entity_settlement.cpp.
struct settlement_building_events
{
	void (*on_added)(match_world_state& world, entity_guid settlement) = nullptr; // After a unit of the type was added to the settlement.
	void (*on_removed)(match_world_state& world, entity_guid settlement) = nullptr; // After a unit of the type was removed from the settlement.
	void (*on_daily_tick)(match_world_state& world, entity_guid settlement) = nullptr; // Once per day, for settlements having at least one unit of the type.
};

// Building type events.

void settlement_building_barracks_on_added(match_world_state& world, entity_guid settlement);
void settlement_building_barracks_on_removed(match_world_state& world, entity_guid settlement);

void settlement_building_tradepost_on_added(match_world_state& world, entity_guid settlement);
void settlement_building_tradepost_on_removed(match_world_state& world, entity_guid settlement);

void settlement_building_tradedepot_on_added(match_world_state& world, entity_guid settlement);
void settlement_building_tradedepot_on_removed(match_world_state& world, entity_guid settlement);

struct settlement_building_def
{
	static constexpr ui8 NAME_MAX = 32;
	static constexpr ui16 DESCRIPTION_MAX = 256;

	char name[NAME_MAX]; // Null terminated. Fixed size so a GDO can cover it (array of I8).
	char description[DESCRIPTION_MAX]; // Null terminated. Generic text for the UI.
	ui16 build_days; // Construction time.
	ui32 cost_per_day; // Wealth per day while under construction.
	ui8 workers; // Workers taken from the peasants while under construction.
	settlement_building_events events;
};

// SETTLEMENT BUILDING TYPES TABLE
// Settlements keep a count per row of this table.
static constexpr settlement_building_def SETTLEMENT_BUILDING_TABLE[] =
{
	{ "Barracks", "Increases Men at Arms Max and trains 1 per day.",
        10, 100, 10, { .on_added = settlement_building_barracks_on_added, .on_removed = settlement_building_barracks_on_removed } },
    { "Caravanserai", "Increases maximum amount of active caravans. Small increase to market goods capacity.",
        10, 100, 25, { .on_added = settlement_building_tradepost_on_added, .on_removed = settlement_building_tradepost_on_removed } },
    { "Trade Depot", "Large increase to market goods capacity.",
        20, 150, 50, { .on_added = settlement_building_tradedepot_on_added, .on_removed = settlement_building_tradedepot_on_removed } },
};

constexpr ui8 SETTLEMENT_BUILDING_TYPE_COUNT = sizeof(SETTLEMENT_BUILDING_TABLE) / sizeof(SETTLEMENT_BUILDING_TABLE[0]);

// A row of the table as a Game Data Object, for the UI. Only what the UI needs: the row's function pointers are not exposed.
GAME_DATA_OBJECT_DEF_BEGIN(settlement_building_def)
	GAME_DATA_PROPERTY_PATH_ARRAY(name, name, GAME_DATA_PROP_TYPE::I8, settlement_building_def::NAME_MAX) // Null terminated text.
	GAME_DATA_PROPERTY_PATH_ARRAY(description, description, GAME_DATA_PROP_TYPE::I8, settlement_building_def::DESCRIPTION_MAX) // Null terminated text.
	GAME_DATA_PROPERTY(build_days, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(cost_per_day, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(workers, GAME_DATA_PROP_TYPE::UI8)
GAME_DATA_OBJECT_DEF_END();
// Training of one troop type in a settlement: every duration_days days, up to amount peasants become soldiers of the type.
struct settlement_troop_training
{
	ui16 amount; // Soldiers per training. 0 = no training.
	ui16 duration_days; // Days between trainings. 0 is treated as 1.
	ui16 progress_days; // Days since the last training.
};

// Last day's economy figures of a settlement, written by the daily economy tick every day. Zero is a valid (empty) value.
struct settlement_economy
{
	ui32 production; // Goods produced by the settlement.
	ui32 production_tax;           // Part of the production moved to the owner's treasury. The rest goes to the market goods.
	ui32 sellable; // Most goods that could be consumed today: sellable_percent of the market goods.
};

// Buildings-related data for a settlement.
struct settlement_buildings
{
    ui16 building_counts[SETTLEMENT_BUILDING_TYPE_COUNT]; // Building count for each type.
};

// Trading-related data for a settlement.
struct settlement_trading
{
    ui8 caravan_count; // Current number of active caravans originating from this settlement.
    ui8 caravan_count_max; // Maximum number of active caravans a settlement can have.
    // Market goods: post-tax production and caravan revenue go in, consumption (sales) and caravan investments come out. Zero by default.
    // Nothing is ever refused: the pool may exceed its capacity, which is only the reference for its fill (settlement_market_capacity).
    ui32 market_goods;
    ui32 market_capacity_buildings; // Buildings' part of the capacity, kept by their added / removed events (constants there).
};

// Property arrays for a world's settlements.
struct world_entity_settlements : public world_entity_container
{
	match_player_id*	owners;
	world_location*		locations;
    ui64*                founding_ticks;

    // Base economic factors.
	ui32* pop_total;

    // POPULATION STRATA

    ui32* pop_unemployed; // Population waiting for a job inside the settlement.

	ui32* pop_peasants; // Rural population working the land held by the settlement.
    ui32* pop_peasants_cap; // Total capacity for peasants in the land held by the settlement.

	ui32* pop_workers; // Population assigned to construction projects, inside or outside the settlement.

    // TROOPS

	army_troop_count* garrison; // Soldiers at home: the local troop pool.
	army_troop_count* raised; // Soldiers in armies raised from the settlement.

	army_troop_count* troop_max; // Max of each soldier type, garrison + raised.
	settlement_troop_training* troop_training[4]; // Indexed by troop type (TROOP_TYPE_*).

	inline ui32 soldiers_home(ui16 index) const { return (ui32)garrison[index].levies + garrison[index].men_at_arms + garrison[index].horsemen + garrison[index].knights; }
	inline ui32 soldiers_raised(ui16 index) const { return (ui32)raised[index].levies + raised[index].men_at_arms + raised[index].horsemen + raised[index].knights; }
	inline ui32 soldiers_total(ui16 index) const { return soldiers_home(index) + soldiers_raised(index); } // Everyone who is a soldier, at home or raised.
	inline ui32 at_home(ui16 index) const { return pop_peasants[index] + pop_workers[index] + soldiers_home(index); }

    // GROWTH

    i32* pop_change;
	i32* pop_growth_per_mille; // Growth rate applied to the population at home, in per mille. Negative = shrinking.

    ui32*               base_land_surplus; // Assuming peasant population at capacity, the total surplus output from land.

	i8*                 tax_rates; // -100..100, % of the daily production moved to the owner's treasury. Negative (future investment) is not applied yet. Reset to 0 on change of owner.

    settlement_buildings* buildings;

    settlement_trading* trade;

    struct territory
    {
        world_location bounds_min, bounds_max; // Rectangle bounds of all currently claimed tiles.
        ui16 tile_count; // Total number of claimed tiles.
    };

	territory* territories; // Influence values used to claim / lose land.

	// Desired counts of the local troop pool (populations[].home).
	army_troop_count*	troop_ceilings;

	// Consolidated representation of a single settlement for viewing purposes.
	struct single
	{
		match_player_id owner;
		world_location location;
        ui64 founding_tick;

        ui32 pop_total;

        ui32 pop_unemployed; // Population waiting for a job inside the settlement.

        ui32 pop_peasants; // Rural population working the land held by the settlement.
        ui32 pop_peasants_cap; // Total capacity for peasants in the land held by the settlement.

        ui32 pop_workers; // Population assigned to construction projects, inside or outside the settlement.

        army_troop_count garrison; // Soldiers at home: the local troop pool.
        army_troop_count raised; // Soldiers in armies raised from the settlement.

        army_troop_count troop_max; // Max of each soldier type, garrison + raised.
        settlement_troop_training troop_training[4]; // Indexed by troop type (TROOP_TYPE_*).

        inline ui32 soldiers_home() const { return (ui32)garrison.levies + garrison.men_at_arms + garrison.horsemen + garrison.knights; }
        inline ui32 soldiers_raised() const { return (ui32)raised.levies + raised.men_at_arms + raised.horsemen + raised.knights; }
        inline ui32 soldiers_total() const { return soldiers_home() + soldiers_raised(); } // Everyone who is a soldier, at home or raised.
        inline ui32 at_home() const { return pop_peasants + pop_workers + soldiers_home(); }

        i32 pop_change;
        i32 pop_growth_per_mille; // Growth rate applied to the population at home, in per mille. Negative = shrinking.

        i8 tax_rate;

        settlement_economy economy;

        territory territory;
        ui32 base_land_output;

        settlement_buildings buildings;

        settlement_trading trade;
        ui32 market_capacity; // Computed (settlement_market_capacity).

		army_troop_count troop_ceilings;
		army_troop_count troop_effective_ceilings;
	};
};

using world_entity_settlement = world_entity_settlements::single;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_settlement)
	GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(population, pop_total, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(population_change, pop_change, GAME_DATA_PROP_TYPE::I32)
	GAME_DATA_PROPERTY_PATH(population_growth_rate, pop_growth_per_mille, GAME_DATA_PROP_TYPE::I32)

	GAME_DATA_PROPERTY_PATH(peasants, pop_peasants, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(workers, pop_workers, GAME_DATA_PROP_TYPE::UI32)

	GAME_DATA_PROPERTY(base_land_output, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(tax_rate, GAME_DATA_PROP_TYPE::I8)

    GAME_DATA_PROPERTY_PATH_ARRAY(building_counts, buildings.building_counts, GAME_DATA_PROP_TYPE::UI16, SETTLEMENT_BUILDING_TYPE_COUNT);

	GAME_DATA_PROPERTY_PATH(economy_production, economy.production, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_sellable, economy.sellable, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY_PATH(trading_caravans_active, trade.caravan_count, GAME_DATA_PROP_TYPE::UI8)
    GAME_DATA_PROPERTY_PATH(trading_caravans_max, trade.caravan_count_max, GAME_DATA_PROP_TYPE::UI8)
    GAME_DATA_PROPERTY_PATH(trading_market_goods, trade.market_goods, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(market_capacity, GAME_DATA_PROP_TYPE::UI32)

	GAME_DATA_PROPERTY_PATH(territory_bounds_min, territory.bounds_min, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(territory_bounds_max, territory.bounds_max, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
    GAME_DATA_PROPERTY_PATH(territory_size, territory.tile_count, GAME_DATA_PROP_TYPE::UI16)

	GAME_DATA_PROPERTY_PATH(raised_levies, raised.levies, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_men_at_arms, raised.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_horsemen, raised.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_knights, raised.knights, GAME_DATA_PROP_TYPE::UI16)

	GAME_DATA_PROPERTY_PATH(troops_men_at_arms, garrison.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_men_at_arms, troop_effective_ceilings.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_men_at_arms, troop_max.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_men_at_arms_amount, troop_training[1].amount, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_men_at_arms_days, troop_training[1].duration_days, GAME_DATA_PROP_TYPE::UI16)

	GAME_DATA_PROPERTY_PATH(troops_horsemen, garrison.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_horsemen, troop_effective_ceilings.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_horsemen, troop_max.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_horsemen_amount, troop_training[2].amount, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_horsemen_days, troop_training[2].duration_days, GAME_DATA_PROP_TYPE::UI16)

	GAME_DATA_PROPERTY_PATH(troops_knights, garrison.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_knights, troop_effective_ceilings.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_knights, troop_max.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_knights_amount, troop_training[3].amount, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_knights_days, troop_training[3].duration_days, GAME_DATA_PROP_TYPE::UI16)

GAME_DATA_OBJECT_DEF_END()

// Query function for Settlement entities.
bool world_entity_query_settlement(const match_world_state& world, entity_guid entity, world_entity_settlement& out_state);

// Returns whether the location is a valid place for the player to build a settlement: buildable terrain, tile unclaimed or owned by the player,
// and at least SETTLEMENT_MIN_DISTANCE tiles away from every settlement.
bool world_is_valid_settlement_location(const match_world_state& world, world_location loc, match_player_id player);

#endif // WORLD_ENTITY_SETTLEMENTS_INCLUDED
