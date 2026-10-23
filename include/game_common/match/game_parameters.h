// Global game parameters: every global constant of the world simulation (time, rates, thresholds), in one place.
// The simulation reads them from GAME_PARAMETERS. A GDO describes the structure so the web client can read the values by name.

#ifndef GAME_PARAMETERS_INCLUDED
#define GAME_PARAMETERS_INCLUDED

#include "core.h"
#include "game_common/game_data_object.h"

// Trade / caravan parameters (placeholder values, tunable). All fields are 4 bytes. See world_trade.h for how the profit ones are used.
struct game_trade_parameters
{
    // Spawning
    ui32 trade_pool_income_percent = 10; // % of the day's income (production + pay + trade) set aside into the settlement's export pool.
    ui32 import_release_percent = 25; // % of the import pool's contents released into the day's income (at least 1 when the pool is not empty).
    ui32 min_export_pool_to_spawn = 1; // A settlement whose export pool is below this spawns no caravan.
    ui32 investment_percent = 25; // % of the origin's export pool invested in a caravan (at least investment_min).
    ui32 investment_min = 100; // Minimum caravan investment, or the whole pool if it holds less.
    ui32 spawn_chance_min_percent = 10; // Spawn probability (%) with 1 missing caravan slot.
    ui32 spawn_chance_max_percent = 50; // Spawn probability (%) at spawn_chance_max_missing_slots missing slots or more. Linear in between.
    ui32 spawn_chance_max_missing_slots = 5;

    // Profit
    ui32 profit_base_multiplier = 2; // Base of the revenue = caravan value * this. (Clamped to 1000 by trade_revenue.) Profit is the extra on top of it.
    ui32 profit_flat_percent = 10; // Flat profit always added, in % of caravan value (at least 1 for any caravan with value). Clamped to 10000.
    ui32 value_bonus_max_percent = 100; // Max value bonus, in % of caravan value (reached when the caravan is tiny next to the destination's daily income). Clamped to 10000.
    ui32 distance_bonus_max_percent = 100; // Max distance bonus, in % of caravan value (reached at the plateau). Clamped to 10000.
                                           //
    // Distances are tiles between origin and destination (trade_tile_distance). A region is 128 tiles wide (world_terrain::REGION_SIZE).
    ui32 distance_curve_mid = 128; // Distance of the curve's "middle" (one region): the bonus rises fast up to about here, slowly after.
    ui32 distance_saturation = 384; // Distance from which the distance bonus stops growing (three regions).
    ui32 distance_curve_steepness = 7; // Higher = faster initial rise. Bonus shape is log2(1 + steepness * distance / curve mid).
};

// Per mille = thousandths (20 = 2%). Ratios are per mille too (1000 = 100%).
// All fields are 4 bytes on purpose: no padding, and the GDO below reads them as UI32 / I32.
struct game_parameters
{
    // TIME
    ui32 ticks_per_day = 100; // A calendar day is this many world ticks. Slow systems (growth...) run once per day.

    // MILITARY
    ui32 raise_min_population_left = 10; // Raising levies / training troops never takes a settlement's peasants below this.
    
    ui8 barracks_maa_max_increase = 50; // How much Men At Arms max is increased per barracks in a settlement.

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
    // r = consumption budget / wealth required for consumption. Growth is in per mille of the population at home per day, linear between these points:
    //   r >= 1000: growth_enough, plus up to (growth_max - growth_enough) as the excess goes from 0 to the requirement.
    //   r == growth_zero_ratio: 0. r <= growth_floor_ratio: growth_floor.
    ui32 consumption_per_person = 1; // Wealth consumed per day by each person at home.
    i32 growth_enough_per_mille = 40; // Growth when consumption is exactly met.
    i32 growth_max_per_mille = 100; // Growth when the excess is at least the requirement.
    ui32 growth_zero_ratio_per_mille = 900; // r at which growth is 0.
    ui32 growth_floor_ratio_per_mille = 500; // r at and below which growth is growth_floor.
    i32 growth_floor_per_mille = -200; // Lowest growth (shrinking).

    // COLONIZATION
    ui32 found_settlement_colonists = 150; // Peasants a founding worker takes from its origin settlement. They become the new settlement's population.

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

    GAME_DATA_PROPERTY_PATH(trade_trade_pool_income_percent, trade.trade_pool_income_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_import_release_percent, trade.import_release_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_min_export_pool_to_spawn, trade.min_export_pool_to_spawn, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_investment_percent, trade.investment_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_investment_min, trade.investment_min, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_spawn_chance_min_percent, trade.spawn_chance_min_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_spawn_chance_max_percent, trade.spawn_chance_max_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_spawn_chance_max_missing_slots, trade.spawn_chance_max_missing_slots, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_profit_base_multiplier, trade.profit_base_multiplier, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_profit_flat_percent, trade.profit_flat_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_value_bonus_max_percent, trade.value_bonus_max_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_distance_bonus_max_percent, trade.distance_bonus_max_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_distance_curve_mid, trade.distance_curve_mid, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_distance_saturation, trade.distance_saturation, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_distance_curve_steepness, trade.distance_curve_steepness, GAME_DATA_PROP_TYPE::UI32)
GAME_DATA_OBJECT_DEF_END()

#endif // GAME_PARAMETERS_INCLUDED
