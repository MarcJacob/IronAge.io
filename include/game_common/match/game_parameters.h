// Global game parameters: every global constant of the world simulation (time, rates, thresholds), in one place.
// The simulation reads them from GAME_PARAMETERS. A GDO describes the structure so the web client can read the values by name.

#ifndef GAME_PARAMETERS_INCLUDED
#define GAME_PARAMETERS_INCLUDED

#include "core.h"
#include "game_common/game_data_object.h"

// @TODO(Marc): Move arable land parameters to params struct.

struct tile_arable_land_def
{
    ui8 peasant_job_count; // Number of full-efficiency peasant jobs available on that tile.
    ui8 surplus_percent; // How much each peasant produces in surplus on that tile assuming no distance penalties, as a percentage.
};

// Number of peasants worth of arable land per terrain type, and the surplus percentage per peasant at 0 distance.
// Each tile can support a certain number of inhabitants natively, and they feed themselves first. Surplus is what is available to be put on the local market.
// When surplus reaches 0 because of distance penalties, the population on the tile is effectively neutral to the settlement's economy and produces no taxable value.
static constexpr tile_arable_land_def TILE_TERRAIN_ARABLE_LAND[] = {
    { 3, 45 }, // LAND_PLAINS
    { 2, 30 }, // LAND_HILLS
    { 1, 30 }, // LAND_MOUNTAINS

    { 5, 45 }, // WATER_COAST
    { 2, 30 }, // WATER_SEA
};

// Distance below max at which surplus is reduced by 10%, stacking additively (IE if surplus is 100, then ten times this value below max distance will wipe surplus).
static constexpr ui8 LAND_SURPLUS_DECREASE_STEP_TRAVEL_DISTANCE = 30;

// Trade / caravan parameters (placeholder values, tunable). All fields are 4 bytes. See world_trade.h for how the profit ones are used.
struct game_trade_parameters
{
    // Market goods sales (daily). Shares are % of the sellable goods; peasants get the rest. Unused shares go to the next stratum (soldiers -> workers -> peasants).
    ui32 sellable_percent = 50; // % of the market goods that can be consumed per day (100 = the pool can be emptied).
    ui32 start_goods_percent = 400; // Default market goods a settlement starts with, in % of its starting population (when its start state gives none).
    ui32 overflow_waste_percent = 25; // % of the goods (local + export pools) above the market capacity that is wasted (removed) each day, after sales and growth.

    // Export pool. export_ratio % of all incoming goods (production, deliveries, refunds) goes to the export pool, the rest to the local market goods.
    ui32 export_ratio_max_percent = 90; // Highest export ratio (%) a player can set. Clamped to 100.
    ui32 start_export_ratio_percent = 20; // Export ratio of new settlements (when their start state gives none).
    ui32 start_caravan_slots = 1; // Max active caravans of new settlements, before buildings (when their start state gives none). Clamped to 255.

    // Caravan Spawning. A caravan takes up to max_caravan_export_pool_percent of the export pool. A candidate destination is eligible if its score
    // (total profit x 100 / tile distance) reaches this, in %. One is picked among the eligible, at random weighted by score.
    ui32 min_profit_per_travel_dist_percent = 50;
    ui32 max_caravan_export_pool_percent = 50; // A caravan takes at most this % of its origin's export pool. Clamped to 100.
    ui32 max_ship_export_pool_percent = 80; // Same for ships (shared export pool, ships spawn after land caravans). Clamped to 100.

    // Profit (trade volume, never goods). Scarcity rate r% of the caravan value, r linear in the destination's fill: max at 0% fill, min at 100%. Clamped to 10000.
    ui32 scarcity_value_min_percent = 25;
    ui32 scarcity_value_max_percent = 150;
    // Profit scaling by (destination capacity - origin capacity) / origin capacity, in %, clamped to [-malus, +bonus]. Malus clamped to 100.
    ui32 capacity_ratio_max_bonus_percent = 100;
    ui32 capacity_ratio_max_malus_percent = 75;
    ui32 incoming_profit_bonus_percent_max = 50; // Profit bonus (%) at the destination's full volume fraction, linear. Clamped to 10000.
    ui32 profit_receiver_percent = 0; // Share (%) of the profit gained by the receiver as volume, the exporter gains the rest. Clamped to 100.
    ui32 distance_margin_min_tiles = 8; // Profit is 0 up to this tile distance, rises linearly to 100% at twice this distance. 0 = no ramp.

    // Trade volume. Max effective volume = volume_max_multiplier x market capacity. Fraction = min(volume, max) / max.
    ui32 volume_max_multiplier = 2;
    ui32 production_bonus_percent_max = 75; // Production bonus (%) at full fraction, linear. Applied before tax. Clamped to 1000.
    ui32 volume_decay_days = 30; // Daily decay = max / this (at least 1 while volume > 0). 0 is treated as 1.

    // Extra margin earned per exchanged good, per specified distance between origin and destination (in number of tiles, "as the crow flies").
    ui16 good_val_markup_increase_dist = 1024;
};

// Per mille = thousandths (20 = 2%). Ratios are per mille too (1000 = 100%).
// All fields are 4 bytes on purpose: no padding, and the GDO below reads them as UI32 / I32.
struct game_parameters
{
    // TIME
    ui32 ticks_per_day = 120; // A calendar day is this many world ticks. Slow systems (growth...) run once per day.

    // BUILDINGS
    ui8 housing_pop_increase = 100; // How much urban population capacity increases per housing.
    ui8 barracks_maa_max_increase = 50; // How much Men At Arms max is increased per barracks in a settlement.

    // MILITARY
    ui32 raise_min_population_left = 10; // Raising levies / training troops never takes a settlement's peasants below this.
    

    // MOVEMENT
    // TEMP: until ships exist. 1 = armies and workers path across water tiles (at their terrain travel cost), 0 = land only. Caravans are always land only, ships water only.
    ui32 armies_workers_cross_water = 1;
    // Speed of each entity kind ON A ROAD, in thousandths of a tile per tick (world_entity_movement::travel_speed). Off-road it is divided by the tile's travel cost.
    ui32 army_travel_speed = 200;
    ui32 worker_travel_speed = 200;
    ui32 caravan_travel_speed = 200;
    ui32 ship_travel_speed = 400; // Ships (caravans with IS_SHIP): twice caravan_travel_speed. Water costs are unchanged (coast 3, sea 7).

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

    // CONSUMPTION & POPULATION GROWTH
    // Per stratum (soldiers at home, workers, peasants): r = its share of the goods sold / goods it required. Growth is in per mille of the stratum per day,
    // linear between these points (excess = share above the requirement):
    //   r >= 1000: growth_enough, plus up to (growth_max - growth_enough) as the excess goes from 0 to the requirement.
    //   r == growth_zero_ratio: 0. r <= growth_floor_ratio: growth_floor.
    i32 growth_enough_per_mille = 20; // Growth when consumption is exactly met.
    i32 growth_max_per_mille = 120; // Growth when the excess is at least the requirement.
    ui32 growth_zero_ratio_per_mille = 800; // r at which growth is 0.
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
    GAME_DATA_PROPERTY(ship_travel_speed, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(upkeep_levy_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(upkeep_men_at_arms, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(upkeep_horseman, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(upkeep_knight, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(army_supply_capacity_days, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(army_supply_refill_days, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(road_crew_cost_per_day, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(road_build_ticks_per_cost, GAME_DATA_PROP_TYPE::UI16)
    GAME_DATA_PROPERTY(road_build_foreign_multiplier, GAME_DATA_PROP_TYPE::UI8)

    GAME_DATA_PROPERTY(growth_enough_per_mille, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY(growth_max_per_mille, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY(growth_zero_ratio_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(growth_floor_ratio_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(growth_floor_per_mille, GAME_DATA_PROP_TYPE::I32)

    GAME_DATA_PROPERTY(found_settlement_colonists, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(tax_rate_min, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY(tax_rate_max, GAME_DATA_PROP_TYPE::I32)

    GAME_DATA_PROPERTY_PATH(trade_sellable_percent, trade.sellable_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_start_goods_percent, trade.start_goods_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_overflow_waste_percent, trade.overflow_waste_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_export_ratio_max_percent, trade.export_ratio_max_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_start_export_ratio_percent, trade.start_export_ratio_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_start_caravan_slots, trade.start_caravan_slots, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_min_profit_per_travel_dist_percent, trade.min_profit_per_travel_dist_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_max_caravan_export_pool_percent, trade.max_caravan_export_pool_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_max_ship_export_pool_percent, trade.max_ship_export_pool_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_scarcity_value_min_percent, trade.scarcity_value_min_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_scarcity_value_max_percent, trade.scarcity_value_max_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_capacity_ratio_max_bonus_percent, trade.capacity_ratio_max_bonus_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_capacity_ratio_max_malus_percent, trade.capacity_ratio_max_malus_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_incoming_profit_bonus_percent_max, trade.incoming_profit_bonus_percent_max, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_profit_receiver_percent, trade.profit_receiver_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_distance_margin_min_tiles, trade.distance_margin_min_tiles, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_volume_max_multiplier, trade.volume_max_multiplier, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_production_bonus_percent_max, trade.production_bonus_percent_max, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_volume_decay_days, trade.volume_decay_days, GAME_DATA_PROP_TYPE::UI32)
GAME_DATA_OBJECT_DEF_END()

#endif // GAME_PARAMETERS_INCLUDED
