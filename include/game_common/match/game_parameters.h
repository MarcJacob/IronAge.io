// Global game parameters: every global constant of the world simulation (time, rates, thresholds), in one place.
// The simulation reads them from GAME_PARAMETERS. A GDO describes the structure so the web client can read the values by name.

#ifndef GAME_PARAMETERS_INCLUDED
#define GAME_PARAMETERS_INCLUDED

#include "core.h"
#include "game_common/game_data_object.h"

// Trade / caravan parameters (placeholder values, tunable). All fields are 4 bytes. See world_trade.h for how the profit ones are used.
struct game_trade_parameters
{
    // Market goods sales (daily). Shares are % of the sellable goods; peasants get the rest. Unused shares go to the next stratum (soldiers -> workers -> peasants).
    ui32 sellable_percent = 50; // % of the market goods that can be consumed per day (100 = the pool can be emptied).
    ui32 soldier_share_percent = 20; // Share of the soldiers at home, served first.
    ui32 worker_share_percent = 20; // Share of the workers, served second.
    ui32 start_goods_percent = 100; // Default market goods a settlement starts with, in % of its starting population (when its start state gives none).
    ui32 overflow_waste_percent = 25; // % of the goods above the market capacity that is wasted (removed) each day, after sales and growth.

    // Spawning
    ui32 min_pool_to_spawn = 100; // A settlement holding fewer market goods than this (absolute amount) spawns no caravan.

    ui32 investment_percent_min = 10; // % of the origin's market goods invested in a caravan at 0% fill (at least investment_min).
    ui32 investment_percent_max = 40; // Same at 100% fill. Linear in between.
    ui32 investment_min = 100; // Minimum caravan investment, or the whole pool if it holds less.

    ui32 spawn_chance_min_percent = 10; // Spawn probability (%) at 0% fill (when the pool is at least min_pool_to_spawn).
    ui32 spawn_chance_max_percent = 100; // Spawn probability (%) at 100% fill. Linear in between.

    // Profit
    ui32 flat_profit_percent_of_min = 10; // Flat profit always added = investment_min * this / 100 (at least 1). Independent of the caravan's value. Clamped to 10000.
    // Barter exchange: the destination values each caravan good at r% of its own goods, r linear in its fill (pool / capacity): max at 0% fill, min at 100%. Clamped to 10000.
    ui32 scarcity_value_min_percent = 50;
    ui32 scarcity_value_max_percent = 200;

    // Extra margin earned per exchanged good, per specified distance between origin and destination (in number of times, "as the crow flies").
    ui16 good_val_markup_increase_dist = 256;
};

// Per mille = thousandths (20 = 2%). Ratios are per mille too (1000 = 100%).
// All fields are 4 bytes on purpose: no padding, and the GDO below reads them as UI32 / I32.
struct game_parameters
{
    // TIME
    ui32 ticks_per_day = 120; // A calendar day is this many world ticks. Slow systems (growth...) run once per day.

    // MILITARY
    ui32 raise_min_population_left = 10; // Raising levies / training troops never takes a settlement's peasants below this.
    
    ui8 barracks_maa_max_increase = 50; // How much Men At Arms max is increased per barracks in a settlement.

    // MOVEMENT
    // TEMP: until ships exist. 1 = armies and workers path across water tiles (at their terrain travel cost), 0 = land only. Caravans are always land only.
    ui32 armies_workers_cross_water = 1;
    // Speed of each entity kind ON A ROAD, in thousandths of a tile per tick (world_entity_movement::travel_speed). Off-road it is divided by the tile's travel cost.
    ui32 army_travel_speed = 200;
    ui32 worker_travel_speed = 200;
    ui32 caravan_travel_speed = 200;

    // ECONOMY
    // Daily army upkeep paid from the owner's treasury, per troop. Levies are in per mille (500 = 0.5), computed on the army's levy total and rounded down.
    ui32 upkeep_levy_per_mille = 500;
    ui32 upkeep_men_at_arms = 2;
    ui32 upkeep_horseman = 10;
    ui32 upkeep_knight = 30;
    ui32 army_supply_capacity_days = 10; // An army refills its supplies (from the treasury, inside its owner's territory) up to this many days of its own consumption.
    ui32 army_supply_refill_days = 2; // Days of its own consumption an army refills per day inside its owner's territory (it consumes 1, so the net gain is refill - 1 days per day).

    // CONSTRUCTION
    ui32 road_crew_cost_per_day = 50; // Wealth paid per day (treasury -> settlement's pending worker pay) while a road worker is building.
    ui16 road_build_ticks_per_cost = 30; // Ticks to build a road tile, per point of its travel cost without a road.
    ui8 road_build_foreign_multiplier = 5; // Build time multiplier for road tiles not owned by the building settlement's player (including unowned tiles).

    // PRODUCTION
    ui8 peasant_production_percent_base = 6; // Percent surplus output per peasant (each peasant will produce at most 1 wealth + surplus%)

    // CONSUMPTION & POPULATION GROWTH
    // Per stratum (soldiers at home, workers, peasants): r = its share of the goods sold / goods it required. Growth is in per mille of the stratum per day,
    // linear between these points (excess = share above the requirement):
    //   r >= 1000: growth_enough, plus up to (growth_max - growth_enough) as the excess goes from 0 to the requirement.
    //   r == growth_zero_ratio: 0. r <= growth_floor_ratio: growth_floor.
    ui32 consumption_per_person = 1; // Market goods consumed per day by each person at home (every stratum).
    i32 growth_enough_per_mille = 75; // Growth when consumption is exactly met.
    i32 growth_max_per_mille = 120; // Growth when the excess is at least the requirement.
    ui32 growth_zero_ratio_per_mille = 750; // r at which growth is 0.
    ui32 growth_floor_ratio_per_mille = 500; // r at and below which growth is growth_floor.
    i32 growth_floor_per_mille = -200; // Lowest growth (shrinking).

    // COLONIZATION
    ui32 found_settlement_colonists = 100; // Peasants a founding worker takes from its origin settlement. They become the new settlement's population.
    ui16 minimum_settlement_distance = 16; // Minimum distance between two settlements in tiles.

    // TAXES
    i32 tax_rate_min = 0; // Lowest tax rate (%) a player can set. Negative would be investment (treasury -> settlement), not applied yet.
    i32 tax_rate_max = 10; // Highest tax rate (%) a player can set.

    // TRADE
    game_trade_parameters trade;
};

static constexpr game_parameters GAME_PARAMETERS = {};

// Not an entity: the GDO covers the parameters structure the same way. Property names are the field names.
GAME_DATA_OBJECT_DEF_BEGIN(game_parameters)
    GAME_DATA_PROPERTY(ticks_per_day, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(raise_min_population_left, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(armies_workers_cross_water, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(army_travel_speed, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(worker_travel_speed, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(caravan_travel_speed, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(upkeep_levy_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(upkeep_men_at_arms, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(upkeep_horseman, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(upkeep_knight, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(army_supply_capacity_days, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(army_supply_refill_days, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(road_crew_cost_per_day, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(road_build_ticks_per_cost, GAME_DATA_PROP_TYPE::UI16)
    GAME_DATA_PROPERTY(road_build_foreign_multiplier, GAME_DATA_PROP_TYPE::UI8)

    GAME_DATA_PROPERTY(peasant_production_percent_base, GAME_DATA_PROP_TYPE::UI8)

    GAME_DATA_PROPERTY(consumption_per_person, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(growth_enough_per_mille, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY(growth_max_per_mille, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY(growth_zero_ratio_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(growth_floor_ratio_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(growth_floor_per_mille, GAME_DATA_PROP_TYPE::I32)

    GAME_DATA_PROPERTY(found_settlement_colonists, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(tax_rate_min, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY(tax_rate_max, GAME_DATA_PROP_TYPE::I32)

    GAME_DATA_PROPERTY_PATH(trade_sellable_percent, trade.sellable_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_soldier_share_percent, trade.soldier_share_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_worker_share_percent, trade.worker_share_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_start_goods_percent, trade.start_goods_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_overflow_waste_percent, trade.overflow_waste_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_min_pool_to_spawn, trade.min_pool_to_spawn, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_investment_percent_min, trade.investment_percent_min, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_investment_percent_max, trade.investment_percent_max, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_investment_min, trade.investment_min, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_spawn_chance_min_percent, trade.spawn_chance_min_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_spawn_chance_max_percent, trade.spawn_chance_max_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_flat_profit_percent_of_min, trade.flat_profit_percent_of_min, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_scarcity_value_min_percent, trade.scarcity_value_min_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_scarcity_value_max_percent, trade.scarcity_value_max_percent, GAME_DATA_PROP_TYPE::UI32)
GAME_DATA_OBJECT_DEF_END()

#endif // GAME_PARAMETERS_INCLUDED
