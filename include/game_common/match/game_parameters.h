// Global game parameters: every global constant of the world simulation (time, rates, thresholds), in one place.
// The simulation reads them from GAME_PARAMETERS. A GDO describes the structure so the web client can read the values by name.

#ifndef GAME_PARAMETERS_INCLUDED
#define GAME_PARAMETERS_INCLUDED

#include "core.h"
#include "game_common/game_data_object.h"
#include "world/troop_types.h"

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
    ui32 caravan_spawn_period_ticks = 20; // Each settlement checks for a caravan / ship spawn when its age (ticks since founding) is a multiple of this. 0 is read as 1.
    ui32 max_caravan_export_pool_percent = 50; // A caravan takes at most this % of its origin's export pool. Clamped to 100.
    ui32 max_ship_export_pool_percent = 80; // Same for ships (shared export pool, ships spawn after land caravans). Clamped to 100.

    // Profit (trade volume, never goods). Scarcity rate r% of the caravan value, r linear in the destination's fill: max at 0% fill, min at 100%. Clamped to 10000.
    ui32 scarcity_value_min_percent = 25;
    ui32 scarcity_value_max_percent = 150;
    ui32 incoming_goods_scarcity_percent = 50; // Goods in flight towards a settlement (incoming_goods) count this % as already in its pool, for the scarcity of profit and caravan weight.
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

// Campaign decision making: objectives on the board, and the scores campaigns rank them with (world_system_campaign_objectives.cpp). Placeholder values, tunable.
// Scores are integers. All fields are 4 bytes.
struct campaign_ai_parameters
{
    ui32 scan_radius_tiles = 100; // Campaigns generate, and consider, objectives within this many tiles of them.
    ui32 campaign_decision_period_ticks = 60; // A campaign ranks its objectives again every this many ticks. 0 is read as 1.

    // Spawning and despawning of each type, every this many ticks (0 is read as 1). An objective despawns once no campaign is committed to it and nothing renewed it
    // (a campaign having it in its scan radius, at a spawn run) for idle_checks despawn runs in a row. Keep the despawn period at least the spawn period.
    ui32 conquer_spawn_period_ticks = 60;
    ui32 conquer_despawn_period_ticks = 60;
    ui32 conquer_despawn_idle_checks = 5;
    ui32 travel_spawn_period_ticks = 60;
    ui32 travel_despawn_period_ticks = 60;
    ui32 travel_despawn_idle_checks = 5;

    ui32 stickiness = 200; // Added to the score of the objective a campaign is already on.

    // Trips (any objective that needs to be walked to, within the scan radius). Time: the cost of the path armies would take, at the army travel speed; the penalty per day applies to all of it,
    // and the low supply one per day of the trip that the supplies of all the member armies together do not cover. A place with no path is not an option.
    ui32 time_penalty_per_day = 50;
    ui32 low_supply_penalty_per_day = 300;

    // WAIT: base, plus per army still on its way to join, plus per day the supplies are short of the comfort value.
    ui32 wait_base = 100;
    ui32 wait_per_joiner = 500;
    ui32 wait_supply_comfort_days = 5;
    ui32 wait_low_supply_per_day = 100;

    // CONQUER_SETTLEMENT: base, minus the trip penalties, minus per % of the campaign's men that the settlement's garrison amounts to,
    // minus per other campaign already pursuing the same objective.
    ui32 conquer_base = 1000;
    ui32 conquer_defender_ratio_penalty = 2;
    ui32 conquer_per_other_campaign_penalty = 200;

    // CONQUER_SETTLEMENT with a target friendly to the campaign = defense. Only eligible while campaigns that are not friendly to the target are pursuing it: base, plus per such
    // campaign, plus a bonus when the settlement is under siege, minus the trip penalties and per other defending campaign (conquer_per_other_campaign_penalty). Placeholders.
    ui32 defend_base = 1200;
    ui32 defend_per_attacker_campaign = 300;
    ui32 defend_siege_relief_bonus = 800;

    // TRAVEL_TO_SETTLEMENT: per tile the settlement brings the campaign closer to the place of its goal (a campaign without one never travels), minus the trip penalties.
    ui32 travel_progress_weight = 15;
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
    ui32 wagon_training_per_day = 1; // Wagons added to a settlement's garrison pool per day, up to the max (garrison + raised), whatever its population.
    ui8 tradepost_wagon_max_increase = 6; // How much the wagons max is increased per caravanserai (tradepost) in a settlement.

    // MILITARY
    ui32 raise_min_population_left = 10; // Training pro troops never takes a settlement's peasants below this.
    ui32 levy_max_percent = 20; // Max levies of a settlement (garrison + raised), in percent of its total population. No ceiling.
    ui32 army_wrong_origin_owner_desertion_percent = 2; // Per day, share of the men of an army whose home settlement now has another owner that deserts (rounded up).
    ui32 army_low_supply_desertion_percent = 10; // Per day, share of an army's max count (raised_troops, per troop type) that deserts when it has no supplies at all, scaled by the share of its consumption supplies could not cover (rounded up, at most its current men). Clamped to 100.
    ui32 levy_training_per_mille = 20; // Levies trained per day, in per mille of the total population (at least 1), up to the max.

    // ARMY STANDING ORDERS (distances in tiles). See world_system_army_standing_orders.cpp.
    ui32 order_attack_reach_tiles = 2; // An army resolves its attack (or chase) on a target within this many tiles.
    ui32 order_station_acquire_radius = 10; // STATION / DEFEND (retreated): hostile armies this close to the army are attacked.
                                            // Army returns to station location if target goes beyond that radius.
    ui32 order_defend_threat_radius = 20; // Hostile armies in the territory or this close to the settlement count in the threat the army weighs itself against.
    ui32 order_raid_flee_radius = 15; // Stronger hostile armies this close make the raiding army flee...
    ui32 order_raid_return_radius = 25; // ... until none is within this radius.
    ui32 order_go_home_disband_tiles = 2; // GO_HOME: the army disbands once this close to its home settlement.

    // ENGAGEMENT JOINING. An army not in an engagement within the join radius of one that is not neutral to its owner joins it (see engagement_join_radius).
    // Radius = base tiles + 1 per power of 2 of the troops involved above the threshold (threshold x 2 -> +1, x 4 -> +2...). Sieges do not count the men inside the settlement.
    ui32 engagement_join_base_tiles = 2;
    ui32 engagement_join_troops_threshold = 256; // At least 1.
    ui32 engagement_pin_distance_tiles = 2; // Armies in an engagement move towards its location until they are this many tiles from it, then stay pinned.
    ui32 engagement_leave_radius_multiplier = 2; // An army farther than this many join radii from its engagement may leave it (see world_engagement_can_be_left).

    // CAMPAIGNS. Join and cluster radius (tiles) = base + per_member x the armies that are members. A campaign follows the centroid of its member armies within that radius.
    ui32 campaign_join_base_tiles = 4;
    ui32 campaign_join_per_member_tiles = 1;
    ui32 campaign_assault_distance_tiles = 10; // MARCHING ends (ON_OBJECTIVE) once the campaign is this close to the objective's location.
    ui32 campaign_assault_outnumber = 3; // A campaign begins the assault of the siege of its objective when the besiegers have this many times the defenders' men (total men)...
    ui32 campaign_assault_min_supply_days = 2; // ...or when one of the besieging armies has fewer days of supplies than this (P5 replaces this rule).
    ui32 campaign_army_garrison_percent = 75; // A settlement joining a campaign raises an army of this % of each of its garrison troop types.

    // ENGAGEMENT LOOT. A conquered settlement adds gold to the engagement's loot pool: people killed, 1 gold each.
    ui32 conquest_loot_peasants_percent = 20; // Of its peasants.
    ui32 conquest_loot_urban_percent = 30; // Of its urban population (unemployed, workers and home soldiers count; only unemployed and workers can actually be killed).

    // ENGAGEMENT DAMAGE. Every engagement_tick_interval ticks a round runs: a side deals (sum over its men of damage) x random percent in [min, max]. The target spreads it
    // over its men by type; each type's share is divided by rand(1, defense) then by rand(1, armor): what is left is the number of men killed of that type.
    ui32 engagement_tick_interval = 20; // World ticks per round. At least 1. (Doubled from 10: battles run at half the speed.)
    ui32 engagement_damage_min_percent = 50;
    ui32 engagement_damage_max_percent = 100;
    ui32 engagement_damage_scale_percent = 5; // Scales all damage dealt per round (100 = 1.0). Tunes battle length.

    // Per troop type, in TROOP_TYPE order (levies, men at arms, horsemen, knights, wagons). Defense and armor are used as at least 1. Damage 0 deals nothing.
    struct troop_type_stat_def
    {
        ui8 damage;
        ui8 defense;
        ui8 armor;
    };
    troop_type_stat_def troop_type_stats[(ui8)TROOP_TYPE::TROOP_TYPE_COUNT] = {
        { 2, 1, 1 },
        { 6, 4, 4 },
        { 4, 4, 2 },
        { 12, 6, 10 },
        { 1, 0, 30 }, // Wagons deal 1 damage: with none, a side left with only wagons would be stuck (see the zero-damage rule in world_system_engagements).
    };

    // ARMY RAISING.
    ui16 raise_pool_draw_per_tick = 3; // Most men of each troop type moved from the settlement into a raising army per tick.

    // SIEGES AND CARAVANS, in tiles.
    ui32 caravan_siege_wait_tiles = 10; // A caravan returning to a besieged origin stops this close to it.
    ui32 caravan_siege_recheck_tiles = 30; // A caravan this close to a besieged destination looks for another one.
    ui32 caravan_army_follow_tiles = 3; // A caravan following an army re-paths once the army is farther than this from the caravan's path destination (or once a day).


    // MOVEMENT
    // TEMP: until ships exist. 1 = armies and workers path across water tiles (at their terrain travel cost), 0 = land only. Caravans are always land only, ships water only.
    ui32 armies_workers_cross_water = 1;

    // Speed of each entity kind with tile travel cost = 1, in thousandths of a tile per tick. 
    // Divided by the tile's travel cost.
    // @TODO(Marc): Move this to troop type stats.
    ui32 army_travel_speed = 150;
    ui32 army_wagon_speed_divisor = 2; // An army containing at least one wagon moves at its normal speed divided by this (stopgap until the army speed rework).
    ui32 worker_travel_speed = 300;
    ui32 caravan_travel_speed = 250;
    ui32 ship_travel_speed = 600;

    // ECONOMY
    // Daily army upkeep, in goods consumed from the army's supplies, per troop. Levies are in per mille (1000 = 1), computed on the army's levy total and rounded down.
    // @TODO(Marc): Move this to troop type stats.
    ui32 upkeep_levy_per_mille = 1000;
    ui32 upkeep_men_at_arms = 2;
    ui32 upkeep_horseman = 3;
    ui32 upkeep_knight = 6;
    ui32 upkeep_wagon = 1;

    // Carry capacity per man. An army's capacity is the sum over its men; the gold pool and the supplies pool each have that capacity.
    // Gold is loot only (never spent), supplies are delivered by caravans (campaign armies) or bought where the army stands, both paid by the owner's treasury, up to it.
    // @TODO(Marc): Move this to troop type stats.
    ui32 carry_levy = 20;
    ui32 carry_men_at_arms = 20;
    ui32 carry_horseman = 32;
    ui32 carry_knight = 30;
    ui32 carry_wagon = 200;

    // RAID LOOT (temporary, until tile-based raiding). Most a settlement can lose to raiders per day, all armies together.
    ui32 raid_production_percent = 10; // Of its production.
    ui32 raid_production_gold_percent = 50; // Share of the production taken that goes to the army's gold (the rest goes to its supplies, as goods).
    ui32 raid_peasants_percent = 10; // Of its peasants, killed (1 gold each).
    ui32 raid_goods_percent = 20; // Of its local + export goods (become supplies 1:1).
    ui32 good_price_gold = 1; // Gold price of one good for army supplies, paid by the army owner's treasury (caravan dispatch, purchase where the army stands).
    ui32 army_friendly_trade_bonus_percent = 25; // Caravans: profit bonus (%) for a friendly army target over a neutral one.

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

    // CAMPAIGN AI
    campaign_ai_parameters campaign_ai;
};

static constexpr game_parameters GAME_PARAMETERS = {};

// Not an entity: the GDO covers the parameters structure the same way. Property names are the field names.
GAME_DATA_OBJECT_DEF_BEGIN(game_parameters)
    GAME_DATA_PROPERTY(ticks_per_day, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(raise_min_population_left, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(levy_max_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(army_wrong_origin_owner_desertion_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(army_low_supply_desertion_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(levy_training_per_mille, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(engagement_join_base_tiles, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(engagement_join_troops_threshold, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(engagement_damage_min_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(engagement_damage_max_percent, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(armies_workers_cross_water, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(army_travel_speed, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(worker_travel_speed, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(caravan_travel_speed, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(ship_travel_speed, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(upkeep_levy_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(upkeep_men_at_arms, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(upkeep_horseman, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(upkeep_knight, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(upkeep_wagon, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(carry_levy, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(carry_men_at_arms, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(carry_horseman, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(carry_knight, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(carry_wagon, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(raid_production_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(raid_production_gold_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(raid_peasants_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(raid_goods_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(good_price_gold, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(army_friendly_trade_bonus_percent, GAME_DATA_PROP_TYPE::UI32)

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
    GAME_DATA_PROPERTY_PATH(trade_caravan_spawn_period_ticks, trade.caravan_spawn_period_ticks, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_max_caravan_export_pool_percent,trade.max_caravan_export_pool_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_max_ship_export_pool_percent, trade.max_ship_export_pool_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_scarcity_value_min_percent, trade.scarcity_value_min_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_scarcity_value_max_percent, trade.scarcity_value_max_percent, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trade_incoming_goods_scarcity_percent, trade.incoming_goods_scarcity_percent, GAME_DATA_PROP_TYPE::UI32)
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
