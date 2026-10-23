// Population systems: settlement expansion pressure, daily economy (wealth, tax, consumption, population growth) and troop pool growth.

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world.h"
#include "game_common/match/world/world_entities.h"

#include "../world_sim.h"
#include "../entities/world_sim_entity_settlement.h"

// Updates settlement pressure based on population and land output.
// Pressure = population - land_output * 3. Positive pressure means surplus population wanting to expand.
void world_system_settlement_territory_update_pressure(match_world_state& world)
{
    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;

        ui32 population = settlements.populations[settlementIndex].population_total;
        ui32 landPopCap = settlements.territories[settlementIndex].tile_count * POP_CAP_PER_TILE;

        i32 newPressure = (i32)population - (i32)(landPopCap);
        settlements.territories[settlementIndex].pressure = newPressure;
    }
}

// DAILY ECONOMY
// Per settlement, in this order: income (production + pending wages / trade + import release), tax, export set-aside, consumption budget, growth.
// Nothing is stored between days: the budget is what the population at home lives on, any excess only adds growth. Every wealth movement is its own function below.

// Peasants able to work the land at full efficiency (base) count fully, the surplus on top of it gives diminishing returns.
static ui32 settlement_raw_output(ui32 base_land_output, ui32 peasants)
{
    if (peasants <= base_land_output) return peasants;
    if (base_land_output == 0) return 0; // No land to work.

    ui32 surplus = peasants - base_land_output;
    return base_land_output + surplus / (1 + surplus / base_land_output);
}

// Wealth produced by a raw land output.
static ui32 land_output_to_wealth(ui32 raw_output)
{
    return raw_output + raw_output / (100 / GAME_PARAMETERS.peasant_production_percent_base);
}

// Wealth needed per day by the population at home (peasants + home soldiers, not raised ones).
static ui32 settlement_consumption_required(const settlement_population& population)
{
    //@TEST(Marc): Need to figure out economy rules that make sense. For now raised population / soldiers does not consume at all from their home settlement,
    // they are paid from the treasury.
    return population.at_home() * GAME_PARAMETERS.consumption_per_person; 
}

// INCOME: land production, plus the external income received since the last daily tick (wages, trade overflow), which is consumed (zeroed) here,
// plus today's release from the import pool.
static void settlement_compute_income(world_entity_settlements& settlements, ui32 settlement_index)
{
    settlement_economy& economy = settlements.economies[settlement_index];
    settlement_pending_income& pending = settlements.pending_incomes[settlement_index];
    settlement_trading& tradeState = settlements.trade[settlement_index];

    economy.raw_output = settlement_raw_output(settlements.base_land_outputs[settlement_index], settlements.populations[settlement_index].peasants);
    economy.production = land_output_to_wealth(economy.raw_output);
    economy.wages = pending.wages;
    economy.trade_overflow = pending.trade;
    pending = {};

    // Import release: a share of the import pool's contents, at least 1 when the pool is not empty and there is a share. Never more than the pool.
    ui32 percent = GAME_PARAMETERS.trade.import_release_percent;
    economy.trade_release = (ui32)ia_min((ui64)tradeState.import_pool * percent / 100, (ui64)tradeState.import_pool);
    if (economy.trade_release == 0 && tradeState.import_pool > 0 && percent > 0) economy.trade_release = 1;
    tradeState.import_pool -= economy.trade_release;

    economy.trade_income = (ui32)ia_min((ui64)economy.trade_overflow + economy.trade_release, (ui64)0xFFFFFFFF);
    economy.income = (ui32)ia_min((ui64)economy.production + economy.wages + economy.trade_income, (ui64)0xFFFFFFFF);
}

// WEALTH MOVEMENT 1: income -> owner's treasury, by the tax rate.
static void settlement_wealth_pay_tax(match_world_state& world, ui32 settlement_index)
{
    world_entity_settlements& settlements = world.entities->settlements;
    i8 taxRate = settlements.tax_rates[settlement_index];
    match_player_id owner = settlements.owners[settlement_index];

    // Negative rates are future investment (treasury -> settlement): not applied yet.
    if (taxRate <= 0 || owner >= world.player_count) return;

    settlement_economy& economy = settlements.economies[settlement_index];
    economy.tax = (ui32)((ui64)economy.income * (ui64)taxRate / 100); // Rate <= 100: never above the income.
    world.player_states[owner].treasury += economy.tax;
    world.player_states[owner].income += economy.tax;
}

// WEALTH MOVEMENT 2: a share of the income -> export pool, taken from what is left after tax.
static void settlement_wealth_set_aside_for_trade(world_entity_settlements& settlements, ui32 settlement_index)
{
    settlement_economy& economy = settlements.economies[settlement_index];

    // Set-aside is a share of the income, at least 1 when there is income and a share.
    ui32 percent = GAME_PARAMETERS.trade.trade_pool_income_percent;
    ui32 setAside = (ui32)ia_min((ui64)economy.income * percent / 100, (ui64)0xFFFFFFFF);
    if (setAside == 0 && economy.income > 0 && percent > 0) setAside = 1;
    // Only what fits under the pool cap is set aside (no Caravanserai: cap 0), the rest stays in the budget.
    settlement_trading& tradeState = settlements.trade[settlement_index];
    ui32 room = tradeState.export_pool_max > tradeState.export_pool ? tradeState.export_pool_max - tradeState.export_pool : 0;
    economy.trade_set_aside = ia_min(ia_min(setAside, economy.income - economy.tax), room);

    tradeState.export_pool += economy.trade_set_aside;
}

// Population growth in per mille of the population at home, from how well the consumption was met. required must not be 0.
// available is the consumption budget, excess what it holds beyond the requirement (not stored).
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

// GROWTH: creates new peasants, or kills peasants only. Never kills more than the peasants or the last one at home.
static void settlement_apply_growth(match_world_state& world, ui32 settlement_index)
{
    world_entity_settlements& settlements = world.entities->settlements;

    settlement_economy& economy = settlements.economies[settlement_index];
    settlement_population& population = settlements.populations[settlement_index];
    ui32 atHome = population.at_home();

    // The budget is what is left of the income: income - tax - set-aside (each was capped by what was left before it).
    economy.consumption_budget = economy.income - economy.tax - economy.trade_set_aside;
    economy.consumption_required = settlement_consumption_required(population);
    if (economy.consumption_required == 0) return; // Nobody at home: nothing to grow or shrink.

    ui32 excess = economy.consumption_budget > economy.consumption_required ? economy.consumption_budget - economy.consumption_required : 0;
    population.growth_per_mille = settlement_growth_per_mille(economy.consumption_budget, economy.consumption_required, excess);

    i32 change = (i32)((i64)atHome * population.growth_per_mille / 1000);
    if (change == 0) change = population.growth_per_mille > 0 ? 1 : (population.growth_per_mille < 0 ? -1 : 0); // Minimum of 1 either way.

    if (change > 0)
    {
        settlement_add_peasants(world, settlement_index, change);
    }
    else if (change < 0)
    {
        ui32 deaths = (ui32)-change;
        if (deaths > atHome - 1) deaths = atHome - 1;
        if (deaths > population.peasants) deaths = population.peasants;
        settlement_remove_peasants(world, settlement_index, deaths);
        change = -(i32)deaths;
    }
    population.population_change = change;
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
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex].population_total == 0) continue;

        settlements.economies[settlementIndex] = {};
 
        // Settlements run no economy logic on their first 10 ticks so they can claim their initial territory.
        if (world.age < settlements.founding_ticks[settlementIndex] + 10) continue; 

        settlement_compute_income(settlements, settlementIndex);
        settlement_wealth_pay_tax(world, settlementIndex);
        settlement_wealth_set_aside_for_trade(settlements, settlementIndex);
        settlement_apply_growth(world, settlementIndex);
    }
}

// Daily training of a troop type: every duration_days days, trains up to the type's amount of peasants into soldiers, limited by the ceiling (already capped
// by max) applied to the total (home + raised), and by the peasants above the minimum. Never removes troops.
// A training that finds no room or no peasants is skipped, the progress resets anyway.
static void train_troop(match_world_state& world, ui16 settlement_index, ui8 troop_type, ui16 ceiling)
{
    settlement_population& population = world.entities->settlements.populations[settlement_index];
    settlement_troop_training& training = population.troop_training[troop_type];

    training.progress_days++;
    if (training.progress_days < (training.duration_days > 0 ? training.duration_days : 1)) return;
    training.progress_days = 0;

    ui32 minLeft = GAME_PARAMETERS.raise_min_population_left;
    ui32 owned = (ui32)troop_count_field(population.home, troop_type) + troop_count_field(population.raised, troop_type);
    ui32 room = owned < ceiling ? ceiling - owned : 0;
    ui32 available = population.peasants > minLeft ? population.peasants - minLeft : 0;
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
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex].population_total == 0) continue;

        settlement_population& population = settlements.populations[settlementIndex];
        army_troop_count ceilings = settlement_troop_effective_ceilings(population, settlements.troop_ceilings[settlementIndex]);

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
