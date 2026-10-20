// Population systems: settlement expansion pressure, daily economy (wealth, tax, consumption, population growth) and troop pool growth.

#include "game_common/match/world/world_entities.h"

#include "../world_sim.h"
#include "world_sim_settlement.h"

// Updates settlement pressure based on population and land output.
// Pressure = population - land_output * 3. Positive pressure means surplus population wanting to expand.
void world_system_economy_update_pressure(match_world_state& world)
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

// Wealth produced by an effective land output. 1 per unit for now. Bonuses (buildings...) plug in here.
static ui32 land_output_to_wealth(ui32 effective_output)
{
    return effective_output;
}

// Wealth needed per day by the population at home (peasants + home soldiers, not raised ones): 1 per person.
static ui32 settlement_consumption_required(const settlement_population& population)
{
    return population.at_home();
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
    world.player_treasuries[owner] += taxed;
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
//  r >= 1: +2% to +4% (more with excess, up to excess == required). 0.9 <= r < 1: +2% down to 0%. 0.5 <= r < 0.9: 0% down to -20%. r < 0.5: -20%.
static i32 settlement_growth_per_mille(ui32 available, ui32 required, ui32 excess)
{
    ui64 avail = available;
    ui64 req = required;

    if (avail >= req) return 20 + (i32)(20 * (ui64)ia_min(excess, required) / req);
    if (avail * 10 >= req * 9) return (i32)((200 * avail - 180 * req) / req);
    if (avail * 2 >= req) return -(i32)((450 * req - 500 * avail) / req);
    return -200;
}

// GROWTH: creates new peasants, or kills peasants only. Never kills more than the peasants or the last one at home.
static void settlement_apply_growth(world_entity_settlements& settlements, ui32 settlement_index)
{
    settlement_finances& finances = settlements.finances[settlement_index];
    settlement_population& population = settlements.populations[settlement_index];
    ui32 atHome = population.at_home();

    if (finances.consumption_required == 0) return; // Nobody at home: nothing to grow or shrink.

    // Available was the local wealth after tax, before consumption.
    ui32 available = finances.consumption_paid + finances.excess;
    finances.growth_per_mille = settlement_growth_per_mille(available, finances.consumption_required, finances.excess);

    i32 change = (i32)((i64)atHome * finances.growth_per_mille / 1000);
    if (change == 0) change = finances.growth_per_mille > 0 ? 1 : (finances.growth_per_mille < 0 ? -1 : 0); // Minimum of 1 either way.

    if (change > 0)
    {
        population.peasants += change;
        population.population_total += change;
    }
    else if (change < 0)
    {
        ui32 deaths = (ui32)-change;
        if (deaths > atHome - 1) deaths = atHome - 1;
        if (deaths > population.peasants) deaths = population.peasants;
        population.peasants -= deaths;
        population.population_total -= deaths;
        change = -(i32)deaths;
    }
    finances.population_change = change;
}

// Runs daily, all settlements on the same tick. Writes the settlement's finances (last day's figures).
void world_system_economy_daily_tick(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex].population_total == 0) continue;

        settlements.finances[settlementIndex] = {};

        settlement_wealth_add_income(settlements, settlementIndex);
        settlement_wealth_pay_tax(world, settlementIndex);
        settlement_wealth_pay_consumption(settlements, settlementIndex);
        settlement_apply_growth(settlements, settlementIndex);
    }
}

// Troops gained per type per daily tick (the former per-run amount of 1, doubled when growth moved to the daily tick).
static constexpr ui16 GROWTH_DAILY_MULTIPLIER = 2;

// Trains peasants into soldiers of a type (up to GROWTH_DAILY_MULTIPLIER), limited by its ceiling and by the peasants above the minimum.
static void train_troop(ui32& peasants, ui16& troops, ui16 ceiling)
{
    ui32 room = troops < ceiling ? ceiling - troops : 0;
    ui32 available = peasants > RAISE_MIN_POPULATION_LEFT ? peasants - RAISE_MIN_POPULATION_LEFT : 0;
    ui32 trained = ia_min(ia_min(room, available), (ui32)GROWTH_DAILY_MULTIPLIER);

    troops += (ui16)trained;
    peasants -= trained;
}

// Local troop pool growth, daily: each troop type gains GROWTH_DAILY_MULTIPLIER per day while below its effective ceiling, trained from peasants (1:1).
// Only while peasants stay above RAISE_MIN_POPULATION_LEFT. Never reduces counts. The total population is unchanged.
void world_system_economy_troop_growth(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex].population_total == 0) continue;

        settlement_population& population = settlements.populations[settlementIndex];
        army_troop_count ceilings = settlement_troop_effective_ceilings(population, settlements.troop_ceilings[settlementIndex]);

        train_troop(population.peasants, population.home.men_at_arms, ceilings.men_at_arms);
        train_troop(population.peasants, population.home.horsemen, ceilings.horsemen);
        train_troop(population.peasants, population.home.knights, ceilings.knights);
    }
}
