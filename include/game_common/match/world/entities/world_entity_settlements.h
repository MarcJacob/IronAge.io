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

void settlement_building_housing_on_added(match_world_state& world, entity_guid settlement);
void settlement_building_housing_on_removed(match_world_state& world, entity_guid settlement);

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
    { "Housing", "Increases population capacity by 100.",
        4, 50, 25, { .on_added = settlement_building_housing_on_added, .on_removed = settlement_building_housing_on_removed } },
	{ "Barracks", "Increases Men at Arms Max and trains 1 per day.",
        10, 100, 50, { .on_added = settlement_building_barracks_on_added, .on_removed = settlement_building_barracks_on_removed } },
    { "Caravanserai", "Increases maximum amount of active caravans. Small increase to market goods capacity.",
        6, 100, 50, { .on_added = settlement_building_tradepost_on_added, .on_removed = settlement_building_tradepost_on_removed } },
    { "Trade Depot", "Large increase to market goods capacity.",
        10, 150, 50, { .on_added = settlement_building_tradedepot_on_added, .on_removed = settlement_building_tradedepot_on_removed } },
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
	ui32 production;        // Goods produced by the settlement.
	ui32 production_tax;    // Part of the production moved to the owner's treasury. The rest goes to the market goods.
	ui32 sellable;          // Most goods that could be consumed today: sellable_percent of the market goods.
    ui32 consumption;       // Goods consumed on last economic tick.
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
    ui8 export_ratio; // Player-set % (0..export_ratio_max_percent) of all incoming goods that goes to the export pool instead of the market goods.
    // Incoming goods (post-tax production, caravan deliveries and refunds) are split by export_ratio between these two pools (settlement_market_add).
    // Market goods (local pool): consumption (sales) comes out. Export goods: only caravans take from it, it is never consumed locally.
    // Nothing is ever refused: the pools may exceed their shared capacity (settlement_market_capacity), the excess of their sum is wasted daily.
    ui32 market_goods;
    ui32 export_goods;
    ui32 market_capacity_buildings; // Buildings' part of the capacity, kept by their added / removed events (constants there).
    // Trade volume: absolute, may exceed its max (settlement_trade_volume_max); only the part up to the max is effective. Decays daily.
    ui32 trade_volume;
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

    ui32* pop_urban_cap; // Total capacity for non-peasant population in the settlement.

    // TROOPS

	army_troop_count* garrison; // Soldiers at home: the local troop pool.
	army_troop_count* raised; // Soldiers in armies raised from the settlement.

	army_troop_count* troop_max; // Max of each soldier type, garrison + raised.
	settlement_troop_training (*troop_training)[4]; // Indexed by troop type (TROOP_TYPE_*).

	inline ui32 soldiers_home(ui16 index) const { return (ui32)garrison[index].levies + garrison[index].men_at_arms + garrison[index].horsemen + garrison[index].knights; }
	inline ui32 soldiers_raised(ui16 index) const { return (ui32)raised[index].levies + raised[index].men_at_arms + raised[index].horsemen + raised[index].knights; }
	inline ui32 soldiers_total(ui16 index) const { return soldiers_home(index) + soldiers_raised(index); } // Everyone who is a soldier, at home or raised.
	inline ui32 at_home(ui16 index) const { return pop_unemployed[index] + pop_peasants[index] + pop_workers[index] + soldiers_home(index); }
    inline ui32 urban_pop(ui16 index) const { return pop_unemployed[index] + pop_workers[index] + soldiers_home(index); }

    // GROWTH

    i32* pop_change;
	i32* pop_growth_per_mille; // Growth rate applied to the population at home, in per mille. Negative = shrinking.

    ui32*               base_land_surplus; // Assuming peasant population at capacity, the total surplus output from land.

	i8*                 tax_rates; // -100..100, % of the daily production moved to the owner's treasury. Negative (future investment) is not applied yet. Reset to 0 on change of owner.

    settlement_buildings* buildings;

    settlement_trading* trade;
    settlement_economy* economies;

    struct territory
    {
        world_location bounds_min, bounds_max; // Rectangle bounds of all currently claimed tiles.
        ui16 tile_count; // Total number of claimed tiles.
        bool distances_stale; // A tile was lost: tile distances must be rebuilt from the settlement tile (cut off tiles are unclaimed).
        bool takeover_pending; // Some tiles have takeover_check set.
    };

	territory* territories; // Influence values used to claim / lose land.

	// Desired counts of the local troop pool.
	army_troop_count*	troop_ceilings;

	struct view;

	// Consolidated representation of a single settlement for viewing purposes.
	struct single
	{
        entity_guid guid;

		match_player_id owner;
		world_location location;
        ui64 founding_tick;

        ui32 pop_total;

        ui32 pop_unemployed; // Population waiting for a job inside the settlement.

        ui32 pop_peasants; // Rural population working the land held by the settlement.
        ui32 pop_peasants_cap; // Total capacity for peasants in the land held by the settlement.

        ui32 pop_workers; // Population assigned to construction projects, inside or outside the settlement.

        ui32 pop_urban_cap; // Total capacity for non-peasant population in the settlement.
                            
        army_troop_count garrison; // Soldiers at home: the local troop pool.
        army_troop_count raised; // Soldiers in armies raised from the settlement.

        army_troop_count troop_max; // Max of each soldier type, garrison + raised.
        settlement_troop_training troop_training[4]; // Indexed by troop type (TROOP_TYPE_*).

        inline ui32 soldiers_home() const { return (ui32)garrison.levies + garrison.men_at_arms + garrison.horsemen + garrison.knights; }
        inline ui32 soldiers_raised() const { return (ui32)raised.levies + raised.men_at_arms + raised.horsemen + raised.knights; }
        inline ui32 soldiers_total() const { return soldiers_home() + soldiers_raised(); } // Everyone who is a soldier, at home or raised.
        inline ui32 at_home() const { return pop_unemployed + pop_peasants + pop_workers + soldiers_home(); }
        inline ui32 urban_pop() const { return pop_unemployed + pop_workers + soldiers_home(); }

        i32 pop_change;
        i32 pop_growth_per_mille; // Growth rate applied to the population at home, in per mille. Negative = shrinking.

        i8 tax_rate;

        settlement_economy economy;

        territory territory;
        ui32 base_land_surplus;

        settlement_buildings buildings;

        settlement_trading trade;
        ui32 market_capacity; // Computed (settlement_market_capacity).
        ui32 trade_volume_max; // Computed (settlement_trade_volume_max).

		army_troop_count troop_ceilings;
		army_troop_count troop_effective_ceilings;

		void copy_from(const view& source);
		void copy_from(const world_entity_settlements& collection, ui16 index);
	};


	// Reference view of a settlement slot. Computed values are not stored in the collection and are not included.
	struct view
	{
        entity_guid& guid;

		match_player_id& owner;
		world_location& location;
        ui64& founding_tick;

        ui32& pop_total;
        ui32& pop_unemployed;
        ui32& pop_peasants;
        ui32& pop_peasants_cap;
        ui32& pop_workers;
        ui32& pop_urban_cap;

        army_troop_count& garrison;
        army_troop_count& raised;
        army_troop_count& troop_max;
        settlement_troop_training (&troop_training)[4]; // Indexed by troop type (TROOP_TYPE_*).

        i32& pop_change;
        i32& pop_growth_per_mille;
        ui32& base_land_surplus;
        i8& tax_rate;

        settlement_buildings& buildings;
        settlement_trading& trade;
        settlement_economy& economy;
        territory& territory;
        army_troop_count& troop_ceilings;

        inline ui32 soldiers_home() const { return (ui32)garrison.levies + garrison.men_at_arms + garrison.horsemen + garrison.knights; }
        inline ui32 soldiers_raised() const { return (ui32)raised.levies + raised.men_at_arms + raised.horsemen + raised.knights; }
        inline ui32 soldiers_total() const { return soldiers_home() + soldiers_raised(); }
        inline ui32 at_home() const { return pop_unemployed + pop_peasants + pop_workers + soldiers_home(); }
        inline ui32 urban_pop() const { return pop_unemployed + pop_workers + soldiers_home(); }

        void copy_from(const single& source)
        {
            guid = source.guid;
            owner = source.owner;
            location = source.location;
            founding_tick = source.founding_tick;
            pop_total = source.pop_total;
            pop_unemployed = source.pop_unemployed;
            pop_peasants = source.pop_peasants;
            pop_peasants_cap = source.pop_peasants_cap;
            pop_workers = source.pop_workers;
            pop_urban_cap = source.pop_urban_cap;
            garrison = source.garrison;
            raised = source.raised;
            troop_max = source.troop_max;
            for (ui8 troopType = 0; troopType < 4; troopType++) troop_training[troopType] = source.troop_training[troopType];
            pop_change = source.pop_change;
            pop_growth_per_mille = source.pop_growth_per_mille;
            base_land_surplus = source.base_land_surplus;
            tax_rate = source.tax_rate;
            buildings = source.buildings;
            trade = source.trade;
            economy = source.economy;
            territory = source.territory;
            troop_ceilings = source.troop_ceilings;
        }

        view(world_entity_settlements& collection, ui16 index)
            : guid(collection.guids[index])
            , owner(collection.owners[index])
            , location(collection.locations[index])
            , founding_tick(collection.founding_ticks[index])
            , pop_total(collection.pop_total[index])
            , pop_unemployed(collection.pop_unemployed[index])
            , pop_peasants(collection.pop_peasants[index])
            , pop_peasants_cap(collection.pop_peasants_cap[index])
            , pop_workers(collection.pop_workers[index])
            , pop_urban_cap(collection.pop_urban_cap[index])
            , garrison(collection.garrison[index])
            , raised(collection.raised[index])
            , troop_max(collection.troop_max[index])
            , troop_training(collection.troop_training[index])
            , pop_change(collection.pop_change[index])
            , pop_growth_per_mille(collection.pop_growth_per_mille[index])
            , base_land_surplus(collection.base_land_surplus[index])
            , tax_rate(collection.tax_rates[index])
            , buildings(collection.buildings[index])
            , trade(collection.trade[index])
            , economy(collection.economies[index])
            , territory(collection.territories[index])
            , troop_ceilings(collection.troop_ceilings[index])
        {
        }
	};
};

inline void world_entity_settlements::single::copy_from(const world_entity_settlements::view& source)
{
	owner = source.owner;
	location = source.location;
	founding_tick = source.founding_tick;
	pop_total = source.pop_total;
	pop_unemployed = source.pop_unemployed;
	pop_peasants = source.pop_peasants;
	pop_peasants_cap = source.pop_peasants_cap;
	pop_workers = source.pop_workers;
    pop_urban_cap = source.pop_urban_cap;
	garrison = source.garrison;
	raised = source.raised;
	troop_max = source.troop_max;
	for (ui8 troopType = 0; troopType < 4; troopType++) troop_training[troopType] = source.troop_training[troopType];
	pop_change = source.pop_change;
	pop_growth_per_mille = source.pop_growth_per_mille;
	tax_rate = source.tax_rate;
	territory = source.territory;
	base_land_surplus = source.base_land_surplus;
	buildings = source.buildings;
	trade = source.trade;
	economy = source.economy;
	market_capacity = 0;
	trade_volume_max = 0;
	troop_ceilings = source.troop_ceilings;
	economy = {};
	troop_effective_ceilings = {};
}

inline void world_entity_settlements::single::copy_from(const world_entity_settlements& collection, ui16 index)
{
	guid = collection.guids[index];
	owner = collection.owners[index];
	location = collection.locations[index];
	founding_tick = collection.founding_ticks[index];
	pop_total = collection.pop_total[index];
	pop_unemployed = collection.pop_unemployed[index];
	pop_peasants = collection.pop_peasants[index];
	pop_peasants_cap = collection.pop_peasants_cap[index];
	pop_workers = collection.pop_workers[index];
    pop_urban_cap = collection.pop_urban_cap[index];
	garrison = collection.garrison[index];
	raised = collection.raised[index];
	troop_max = collection.troop_max[index];
	for (ui8 troopType = 0; troopType < 4; troopType++) troop_training[troopType] = collection.troop_training[index][troopType];
	pop_change = collection.pop_change[index];
	pop_growth_per_mille = collection.pop_growth_per_mille[index];
	tax_rate = collection.tax_rates[index];
	territory = collection.territories[index];
	base_land_surplus = collection.base_land_surplus[index];
	buildings = collection.buildings[index];
	trade = collection.trade[index];
	economy = collection.economies[index];
	market_capacity = 0;
	trade_volume_max = 0;
	troop_ceilings = collection.troop_ceilings[index];
	economy = collection.economies[index];
	troop_effective_ceilings = collection.troop_ceilings[index];
}

using world_entity_settlement = world_entity_settlements::single;
using world_entity_settlement_view = world_entity_settlements::view;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_settlement)

    GAME_DATA_PROPERTY(guid, GAME_DATA_PROP_TYPE::ENTITY_GUID)

	GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)

	GAME_DATA_PROPERTY_PATH(population, pop_total, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(population_change, pop_change, GAME_DATA_PROP_TYPE::I32)
	GAME_DATA_PROPERTY_PATH(population_growth_rate, pop_growth_per_mille, GAME_DATA_PROP_TYPE::I32)

    GAME_DATA_PROPERTY(pop_unemployed, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(pop_peasants, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(pop_workers, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(pop_urban_cap, GAME_DATA_PROP_TYPE::UI32)

	GAME_DATA_PROPERTY(base_land_surplus, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(tax_rate, GAME_DATA_PROP_TYPE::I8)

    GAME_DATA_PROPERTY_PATH_ARRAY(building_counts, buildings.building_counts, GAME_DATA_PROP_TYPE::UI16, SETTLEMENT_BUILDING_TYPE_COUNT);

	GAME_DATA_PROPERTY_PATH(economy_production, economy.production, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_tax, economy.production_tax, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_sellable, economy.sellable, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_consumption, economy.consumption, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY_PATH(trading_caravans_active, trade.caravan_count, GAME_DATA_PROP_TYPE::UI8)
    GAME_DATA_PROPERTY_PATH(trading_caravans_max, trade.caravan_count_max, GAME_DATA_PROP_TYPE::UI8)
    GAME_DATA_PROPERTY_PATH(trading_market_goods, trade.market_goods, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trading_export_goods, trade.export_goods, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trading_export_ratio, trade.export_ratio, GAME_DATA_PROP_TYPE::UI8)

    GAME_DATA_PROPERTY_PATH(trading_trade_volume, trade.trade_volume, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(market_capacity, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(trade_volume_max, GAME_DATA_PROP_TYPE::UI32)

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
