// Global game parameters: every global constant of the world simulation (time, rates, thresholds), in one place.
// The simulation reads them from GAME_PARAMETERS. A GDO describes the structure so the web client can read the values by name.

#ifndef GAME_PARAMETERS_INCLUDED
#define GAME_PARAMETERS_INCLUDED

#include "core.h"
#include "game_common/game_data_object.h"

// Per mille = thousandths (20 = 2%). Ratios are per mille too (1000 = 100%).
// All fields are 4 bytes on purpose: no padding, and the GDO below reads them as UI32 / I32.
struct game_parameters
{
    // TIME
    ui32 ticks_per_day; // A calendar day is this many world ticks. Slow systems (growth...) run once per day.

    // TROOPS
    ui32 raise_min_population_left; // Raising levies / training troops never takes a settlement's peasants below this.
    ui32 troop_growth_per_day; // Troops gained per type per day in a settlement's local pool, trained from peasants (1:1).
    ui32 men_at_arms_max_per_mille; // Max local men at arms, as a share of the settlement's total population.
    ui32 horsemen_max_per_mille; // Same, for horsemen.
    ui32 knights_max_per_mille; // Same, for knights. (Levies are not pooled: their max is peasants - raise_min_population_left.)

    // CONSUMPTION & POPULATION GROWTH
    // r = wealth available / wealth required for consumption. Growth is in per mille of the population at home per day, linear between these points:
    //   r >= 1000: growth_enough, plus up to (growth_max - growth_enough) as the excess goes from 0 to the requirement.
    //   r == growth_zero_ratio: 0. r <= growth_floor_ratio: growth_floor.
    ui32 consumption_per_person; // Wealth consumed per day by each person at home.
    i32 growth_enough_per_mille; // Growth when consumption is exactly met.
    i32 growth_max_per_mille; // Growth when the excess is at least the requirement.
    ui32 growth_zero_ratio_per_mille; // r at which growth is 0.
    ui32 growth_floor_ratio_per_mille; // r at and below which growth is growth_floor.
    i32 growth_floor_per_mille; // Lowest growth (shrinking).
};

static constexpr game_parameters GAME_PARAMETERS = {
    .ticks_per_day = 100,

    .raise_min_population_left = 10,
    .troop_growth_per_day = 2,
    .men_at_arms_max_per_mille = 20,
    .horsemen_max_per_mille = 10,
    .knights_max_per_mille = 5,

    .consumption_per_person = 1,
    .growth_enough_per_mille = 20,
    .growth_max_per_mille = 40,
    .growth_zero_ratio_per_mille = 900,
    .growth_floor_ratio_per_mille = 500,
    .growth_floor_per_mille = -200,
};

// Not an entity: the GDO covers the parameters structure the same way. Property names are the field names.
GAME_DATA_OBJECT_DEF_BEGIN(game_parameters)
    GAME_DATA_PROPERTY(ticks_per_day, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(raise_min_population_left, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(troop_growth_per_day, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(men_at_arms_max_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(horsemen_max_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(knights_max_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(consumption_per_person, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(growth_enough_per_mille, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY(growth_max_per_mille, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY(growth_zero_ratio_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(growth_floor_ratio_per_mille, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(growth_floor_per_mille, GAME_DATA_PROP_TYPE::I32)
GAME_DATA_OBJECT_DEF_END()

#endif // GAME_PARAMETERS_INCLUDED
