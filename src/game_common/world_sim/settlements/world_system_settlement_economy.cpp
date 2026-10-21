// Population systems: settlement expansion pressure, daily economy (wealth, tax, consumption, population growth) and troop pool growth.

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world.h"
#include "game_common/match/world/world_entities.h"

#include "../world_sim.h"
#include "world_sim_entity_settlement.h"

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
// Per settlement, in this order: output -> wealth, tax, consumption, growth. Every wealth movement is its own function below.

// Peasants able to work the land at full efficiency (base) count fully, the surplus on top of it gives diminishing returns.
static ui32 settlement_effective_output(ui32 base_land_output, ui32 peasants)
{
    if (peasants <= base_land_output) return peasants;
    if (base_land_output == 0) return 0; // No land to work.

    ui32 surplus = peasants - base_land_output;
    return base_land_output + surplus / (1 + surplus / base_land_output);
}

// Wealth produced by an effective land output.
static ui32 land_output_to_wealth(ui32 effective_output)
{
    return effective_output + effective_output / (100 / GAME_PARAMETERS.peasant_production_percent_base);
}

// Wealth needed per day by the population at home (peasants + home soldiers, not raised ones).
static ui32 settlement_consumption_required(const settlement_population& population)
{
    //@TEST(Marc): Need to figure out economy rules that make sense. For now raised population / soldiers does not consume at all from their home settlement,
    // they are paid from the treasury.
    return population.at_home() * GAME_PARAMETERS.consumption_per_person; 
}

// WEALTH MOVEMENT 1: land output -> local wealth.
static void settlement_wealth_add_income(world_entity_settlements& settlements, ui32 settlement_index)
{
    settlement_finances& finances = settlements.finances[settlement_index];
    finances.effective_output = settlement_effective_output(settlements.base_land_outputs[settlement_index], settlements.populations[settlement_index].peasants);
    finances.income = land_output_to_wealth(finances.effective_output);
    settlements.local_wealth[settlement_index] += finances.income;
}

// WEALTH MOVEMENT 2: local wealth -> owner's treasury, by the tax rate.
static void settlement_wealth_pay_tax(match_world_state& world, ui32 settlement_index)
{
    world_entity_settlements& settlements = world.entities->settlements;
    i8 taxRate = settlements.tax_rates[settlement_index];
    match_player_id owner = settlements.owners[settlement_index];

    // Negative rates are future investment (treasury -> settlement): not applied yet.
    if (taxRate <= 0 || owner >= world.player_count) return;

    ui32 taxed = (ui32)((ui64)settlements.local_wealth[settlement_index] * (ui64)taxRate / 100);
    settlements.local_wealth[settlement_index] -= taxed;
    world.player_states[owner].treasury += taxed;
    world.player_states[owner].income += taxed;
    settlements.finances[settlement_index].tax_paid = taxed;
}

// WEALTH MOVEMENT 3: local wealth consumed by the population at home. Whatever is available beyond the requirement stays as local wealth (excess).
static void settlement_wealth_pay_consumption(world_entity_settlements& settlements, ui32 settlement_index)
{
    settlement_finances& finances = settlements.finances[settlement_index];
    ui32 available = settlements.local_wealth[settlement_index];

    finances.consumption_required = settlement_consumption_required(settlements.populations[settlement_index]);
    finances.consumption_paid = ia_min(available, finances.consumption_required);
    finances.excess = available - finances.consumption_paid;
    settlements.local_wealth[settlement_index] -= finances.consumption_paid;
}

// Population growth in per mille of the population at home, from how well the consumption was met. required must not be 0.
// available is the local wealth after tax (before consumption), excess what was left of it after consumption.
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

    settlement_finances& finances = settlements.finances[settlement_index];
    settlement_population& population = settlements.populations[settlement_index];
    ui32 atHome = population.at_home();

    if (finances.consumption_required == 0) return; // Nobody at home: nothing to grow or shrink.

    // Available was the local wealth after tax, before consumption.
    ui32 available = finances.consumption_paid + finances.excess;
    population.growth_per_mille = settlement_growth_per_mille(available, finances.consumption_required, finances.excess);

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

// Runs daily, all settlements on the same tick. Writes the settlement's finances (last day's figures).
void world_system_economy_daily_tick(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    world_entity_settlements& settlements = world.entities->settlements;

    // Player figures are last day's: reset, then every deposit adds to income. Future spending from the treasury adds to expenses.
    for (ui32 playerIndex = 0; playerIndex < world.player_count; playerIndex++)
    {
        world.player_states[playerIndex].income = 0;
        world.player_states[playerIndex].expenses = 0;
    }

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex].population_total == 0) continue;

        settlements.finances[settlementIndex] = {};
 
        // Settlements run no economy logic on their first 10 ticks so they can claim their initial territory.
        if (world.age < settlements.founding_ticks[settlementIndex] + 10) continue; 

        settlement_wealth_add_income(settlements, settlementIndex);
        settlement_wealth_pay_tax(world, settlementIndex);
        settlement_wealth_pay_consumption(settlements, settlementIndex);
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

        for (ui8 buildingType = 0; buildingType < SETTLEMENT_BUILDING_TYPE_COUNT; buildingType++)
        {
            auto onDailyTick = SETTLEMENT_BUILDING_TABLE[buildingType].events.on_daily_tick;
            if (onDailyTick && settlements.building_count(settlementIndex, buildingType) > 0)
            {
                onDailyTick(world, settlements.guids[settlementIndex]);
            }
        }
    }
}
