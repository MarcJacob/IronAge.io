// Population systems: settlement expansion pressure, daily economy (wealth, tax, consumption, population growth) and troop pool growth.

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/entities/world_entity_settlements.h"
#include "game_common/match/world/world_state.h"

#include "../world_sim.h"
#include "../entities/world_sim_entity_settlement.h"

// DAILY ECONOMY
// Per settlement, in this order: production, tax on it, the rest into the market goods, sales of the sellable goods to the strata, growth of each stratum.

// Peasants able to work the land at full efficiency (base) count fully, the surplus on top of it gives diminishing returns.
static ui32 settlement_raw_output(ui32 base_land_output, ui32 peasants)
{
    if (peasants <= base_land_output) return peasants;
    if (base_land_output == 0) return 0; // No land to work.

    // Surplus gives B * s / (B + s): smooth, monotonic, tends to B (so the total tends to 2B). 64 bits since B * s can pass 32.
    ui64 surplus = peasants - base_land_output;
    return base_land_output + (ui32)(surplus * base_land_output / (base_land_output + surplus));
}

// Wealth produced by a raw land output.
static ui32 land_output_to_wealth(ui32 raw_output)
{
    return raw_output + raw_output / (100 / GAME_PARAMETERS.peasant_production_percent_base);
}

// PRODUCTION: the owner's treasury takes the tax rate of the production, the rest goes into the market goods.
static void settlement_goods_production(match_world_state& world, ui16 settlement_index)
{
    world_entity_settlement_view settlement { world.entities->settlements, settlement_index };

    ui16 peasantsFillPerMille = settlement.pop_peasants * 1000 / settlement.pop_peasants_cap;
    settlement.economy.production = settlement.base_land_surplus * ia_min(peasantsFillPerMille, 1000);

    // Negative rates are future investment (treasury -> settlement): not applied yet.
    match_player_id owner = settlement.owner;
    if (settlement.tax_rate > 0 && owner < world.player_count)
    {
        settlement.economy.production_tax = (ui32)((ui64)settlement.economy.production * (ui64)settlement.tax_rate / 100); // Rate <= 100: never above the production.
        world.player_states[owner].treasury += settlement.economy.production_tax;
        world.player_states[owner].income += settlement.economy.production_tax;
    }

    settlement_market_add(world.entities->settlements, settlement_index, settlement.economy.production - settlement.economy.production_tax);
}

// SALES: sellable_percent of the market goods can be consumed today. Soldiers at home, then workers, are offered their share of it; what a stratum leaves
// of its share goes to the next one. Peasants are offered everything left. Only what the strata take leaves the market goods.
static void settlement_goods_sales(world_entity_settlements& settlements, ui16 settlement_index)
{
    const game_trade_parameters& params = GAME_PARAMETERS.trade;

    world_entity_settlement_view settlement { settlements, settlement_index };

    ui32& goods = settlements.trade[settlement_index].market_goods;

    settlement.economy.sellable = (ui32)((ui64)goods * ia_min(params.sellable_percent, (ui32)100) / 100);
    settlement.economy.consumption = settlement.soldiers_home() + settlement.pop_workers + settlement.pop_unemployed;
    goods -= settlement.economy.consumption;

    // Peasants do not consume anything, they feed themselves from the land. They do contribute to population growth and thus benefit from extra goods in that way.
}

// Population growth in per mille, from how well consumption demand was met. required must not be 0.
// available is what was offered, excess what it holds beyond the requirement (not consumed).
// Curve points are in GAME_PARAMETERS (now: r >= 1: +2% to +4%, more with excess up to excess == required; 0.9 <= r < 1: +2% down to 0%;
// 0.5 <= r < 0.9: 0% down to -20%; r < 0.5: -20%).
static i32 settlement_growth_per_mille(ui32 available, ui32 required, ui32 excess)
{
    const game_parameters& params = GAME_PARAMETERS;
    ui64 avail = available;
    ui64 req = required;
    ui64 zeroRatio = params.growth_zero_ratio_per_mille;
    ui64 floorRatio = params.growth_floor_ratio_per_mille;

    if (avail >= req) return params.growth_enough_per_mille + (i32)((ui64)(params.growth_max_per_mille - params.growth_enough_per_mille) * ia_min(excess, required) / req);
    if (avail * 1000 >= req * zeroRatio) return (i32)((ui64)params.growth_enough_per_mille * (avail * 1000 - req * zeroRatio) / (req * (1000 - zeroRatio)));
    if (avail * 1000 >= req * floorRatio) return -(i32)((ui64)(-params.growth_floor_per_mille) * (req * zeroRatio - avail * 1000) / (req * (zeroRatio - floorRatio)));
    return params.growth_floor_per_mille;
}

// Deaths of home soldiers: each type loses its share of the count (rounded down), the rounding rest is taken in type order. Returns how many died.
static ui32 settlement_soldiers_home_die(match_world_state& world, ui16 settlement_index, ui32 count)
{
    world_entity_settlement_view settlement { world.entities->settlements, settlement_index };

    count = ia_min(count, settlement.soldiers_home());
    if (count == 0) return 0;

    ui32 removed = 0;
    for (ui8 troopType = 0; troopType < TROOP_TYPE_COUNT; troopType++)
    {
        removed += settlement_remove_soldiers_home(world, settlement_index, troopType, (ui16)((ui64)troop_count_field(settlement.garrison, troopType) * count / settlement.soldiers_home()));
    }
    for (ui8 troopType = 0; troopType < TROOP_TYPE_COUNT && removed < count; troopType++)
    {
        removed += settlement_remove_soldiers_home(world, settlement_index, troopType, (ui16)ia_min(count - removed, (ui32)0xFFFF));
    }
    return removed;
}

// GROWTH, per stratum (soldiers at home, workers, peasants), from how well its need was met.
// New population starts out unemployed.
static void settlement_apply_growth(match_world_state& world, ui16 settlement_index)
{
    world_entity_settlement_view settlement { world.entities->settlements, settlement_index };

    ui32 atHome = settlement.at_home();
    if (atHome == 0) return; // Nobody at home: nothing to grow or shrink.

    settlement.pop_growth_per_mille = settlement.economy.sellable * 1000 / settlement.economy.consumption;

    settlement.pop_change = (settlement.pop_total * 1000 / settlement.pop_growth_per_mille) / 1000;

    if (settlement.pop_change > 0)
    {
        settlement.pop_unemployed += settlement.pop_change;
    }
    else
    {
        ui32 deaths = -settlement.pop_change;
        ui16 deaths_unemployed = ia_min(settlement.pop_unemployed, deaths);

        settlement.pop_unemployed -= deaths_unemployed;
        if (deaths > deaths_unemployed)
        {
            ui32 deaths_peasants = settlement_remove_peasants(world, settlement_index, deaths - deaths_unemployed);
            if (deaths > deaths_unemployed + deaths_peasants)
            {
                ui32 deaths_workers = settlement_remove_workers(world, settlement_index, deaths - deaths_unemployed - deaths_peasants);

                // @TEMP(Marc): Garrison troops never die for now.
            }
        }
    }
}

// WASTE: overflow_waste_percent of the goods above the market capacity (rounded down) disappears. Run after growth, so the capacity is the settled one.
static void settlement_goods_waste(world_entity_settlements& settlements, ui32 settlement_index)
{
    ui32& goods = settlements.trade[settlement_index].market_goods;
    ui32 capacity = settlement_market_capacity(settlements, (ui16)settlement_index);
    if (goods <= capacity) return;

    goods -= (ui32)((ui64)(goods - capacity) * ia_min(GAME_PARAMETERS.trade.overflow_waste_percent, (ui32)100) / 100);
}

// Runs daily, all settlements on the same tick. Writes the settlement's economy figures (last day's).
void world_system_economy_daily_tick(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    world_entity_settlements& settlements = world.entities->settlements;

    // Player figures are last day's: reset, then every deposit adds to income. Spending from the treasury adds to expenses (the worker system runs after this).
    for (ui32 playerIndex = 0; playerIndex < world.player_count; playerIndex++)
    {
        world.player_states[playerIndex].income = 0;
        world.player_states[playerIndex].expenses = 0;
    }

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;
        if (settlements.at_home(settlementIndex) == 0) continue; // @TODO(Marc): Destroy settlement ?

        settlements.economies[settlementIndex] = {};
 
        // Settlements run no economy logic on their first 10 ticks so they can claim their initial territory.
        if (world.age < settlements.founding_ticks[settlementIndex] + 10) continue; 

        settlement_goods_production(world, settlementIndex);
        settlement_goods_sales(settlements, settlementIndex);
        settlement_apply_growth(world, settlementIndex);
        settlement_goods_waste(settlements, settlementIndex);
    }
}

// Daily training of a troop type: every duration_days days, trains up to the type's amount of peasants into soldiers, limited by the ceiling (already capped
// by max) applied to the total (home + raised), and by the peasants above the minimum. Never removes troops.
// A training that finds no room or no peasants is skipped, the progress resets anyway.
static void train_troop(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 ceiling)
{
    world_entity_settlement_view settlement { world.entities->settlements, settlement_index };

    settlement_troop_training& training = settlement.troop_training[troop_type];

    training.progress_days++;
    if (training.progress_days < (training.duration_days > 0 ? training.duration_days : 1)) return;
    training.progress_days = 0;

    ui32 minLeft = GAME_PARAMETERS.raise_min_population_left;
    ui32 owned = (ui32)troop_count_field(settlement.garrison, troop_type) + troop_count_field(settlement.raised, troop_type);
    ui32 room = owned < ceiling ? ceiling - owned : 0;
    ui32 available = settlement.pop_peasants > minLeft ? settlement.pop_peasants - minLeft : 0;
    ui32 trained = ia_min(ia_min(room, available), (ui32)training.amount);

    settlement_train_soldiers(world, settlement_index, troop_type, trained);
}

// Local troop pool growth, daily: each troop type is trained from peasants (1:1) following the settlement's training values, while home + raised is below
// its effective ceiling. Only while peasants stay above raise_min_population_left. Never reduces counts. The total population is unchanged.
void world_system_economy_troop_growth(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.pop_total == 0) continue;

        army_troop_count ceilings = settlement_troop_effective_ceilings(settlements.troop_max[settlementIndex], settlements.troop_max[settlementIndex]);

        train_troop(world, settlementIndex, TROOP_TYPE_MEN_AT_ARMS, ceilings.men_at_arms);
        train_troop(world, settlementIndex, TROOP_TYPE_HORSEMEN, ceilings.horsemen);
        train_troop(world, settlementIndex, TROOP_TYPE_KNIGHTS, ceilings.knights);
    }
}

void world_system_settlement_buildings_daily_tick(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;

        for (ui8 buildingTypeIndex = 0; buildingTypeIndex < SETTLEMENT_BUILDING_TYPE_COUNT; buildingTypeIndex++)
        {
            auto onDailyTick = SETTLEMENT_BUILDING_TABLE[buildingTypeIndex].events.on_daily_tick;
            if (onDailyTick && settlements.buildings[settlementIndex].building_counts[buildingTypeIndex] > 0)
            {
                onDailyTick(world, settlements.guids[settlementIndex]);
            }
        }
    }
}
