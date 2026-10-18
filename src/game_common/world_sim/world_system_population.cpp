// Population systems: settlement expansion pressure and population growth.

#include "game_common/match/world.h"
#include "world_sim.h"
#include "world_sim_population.h"

// Updates settlement pressure based on population and land output.
// Pressure = population - land_output * 3. Positive pressure means surplus population wanting to expand.
void world_system_economy_update_pressure(match_world_state& world)
{
    world_entity_settlements& settlements = world.entities.settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;

        ui32 population = settlements.populations[settlementIndex];
        ui32 landPopCap = settlements.land_sizes[settlementIndex] * POP_CAP_PER_TILE;

        i32 newPressure = (i32)population - (i32)(landPopCap);
        settlements.influences[settlementIndex].pressure = newPressure;
    }
}

void world_system_economy_pop_growth(match_world_state& world)
{
    world_entity_settlements& settlements = world.entities.settlements;

    // POP GROWTH
    // Population seeks to reach an equilibrium that depends on settlement land / food output and by local wealth.
    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex] == 0) continue;

        // @TEMP(Marc): Crude spread / optimization of work: Run on each settlement every 10 ticks based on their Extra guid field and world age.
        if ((settlements.guids[settlementIndex].get_extra() + world.age) % 10 != 0) continue;

        ui32 targetPopulation = settlements.land_outputs[settlementIndex] / 10;

        ui32& population = settlements.populations[settlementIndex];

        i32 growth = ((i64)targetPopulation - (i64)population);
        if (growth > 0 && (ui32)growth >= 1 + population / 100)
        {
            growth = 1 + (i64)population / 100; // Population cannot grow by more than 1% per growth tick.
        }
        else if (growth < 0 && ia_abs(growth) > (i64)population / 20)
        {
            growth = -(i64)population / 20; // Population cannot shrink by more than 5% per growth tick.
        }

        if ((i64)population + growth < 1)
        {
            population = 1;
        }
        else
        {
            population += growth;
        }

    }
}
