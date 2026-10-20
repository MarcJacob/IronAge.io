// Population systems: settlement expansion pressure and population growth.

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

        ui32 population = settlements.populations[settlementIndex];
        ui32 landPopCap = settlements.territories[settlementIndex].tile_count * POP_CAP_PER_TILE;

        i32 newPressure = (i32)population - (i32)(landPopCap);
        settlements.territories[settlementIndex].pressure = newPressure;
    }
}

// Runs daily.
void world_system_economy_pop_growth(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    world_entity_settlements& settlements = world.entities->settlements;

    // POP GROWTH
    // Population seeks to reach an equilibrium that depends on settlement land / food output and by local wealth.
    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex] == 0) continue;

        ui32 targetPopulation = settlements.land_outputs[settlementIndex] / 10;

        ui32& population = settlements.populations[settlementIndex];

        constexpr ui8 POP_GROWTH_LOW_BONUS = 3;

        i32 growth = ia_min((i64)targetPopulation - (i64)population, population); // Base growth is simply difference between current and target population, or population, whichever is lower.

        if (growth > 0) // Population growing / Stagnating.
        {
            // Place growth on a log curve of sorts, benefiting it for values whose square roots are under a certain value.
            growth = POP_GROWTH_LOW_BONUS + ia_sqrt(growth, 10) * POP_GROWTH_LOW_BONUS; // @TODO: Get int-based square root function to guarantee determinism. In practice for now this should work well-enough.
        }
        else // Population shrinking.
        {
            const i32 MAX_SHRINK = (i64)population * 20 / 100; // Population cannot shrink by more than 20% per day.
            if (growth < 0 && ia_abs(growth) > MAX_SHRINK)
            {
                growth = -MAX_SHRINK; 
            }
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

// Local troop pool growth, daily: each troop type gains GROWTH_DAILY_MULTIPLIER per day while below its effective ceiling. Never reduces counts.
void world_system_economy_troop_growth(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex] == 0) continue;

        army_troop_count ceilings = settlement_troop_effective_ceilings(settlements.populations[settlementIndex], settlements.troop_ceilings[settlementIndex]);
        army_troop_count& troops = settlements.local_troops[settlementIndex];

        // Gain is capped at the ceiling.
        if (troops.men_at_arms < ceilings.men_at_arms) troops.men_at_arms = ia_min(troops.men_at_arms + 1, ceilings.men_at_arms);
        if (troops.horsemen < ceilings.horsemen) troops.horsemen = ia_min(troops.horsemen + 1, ceilings.horsemen);
        if (troops.knights < ceilings.knights) troops.knights = ia_min(troops.knights + 1, ceilings.knights);
    }
}
