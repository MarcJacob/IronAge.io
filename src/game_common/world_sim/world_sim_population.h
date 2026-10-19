// Population symbols shared between world simulation themes.

#ifndef WORLD_SIM_POPULATION_INCLUDED
#define WORLD_SIM_POPULATION_INCLUDED

#include "core/std_types.h"
#include "game_common/match/world.h"

// How much population "Fills" one tile of terrain.
// If population is higher than this, the settlement gets pressure to expand.
static const ui8 POP_CAP_PER_TILE = 2;

// Raising levies never takes a settlement's population below this.
static constexpr ui32 RAISE_MIN_POPULATION_LEFT = 10;

// How many armies a settlement can have raised at once. A constant for now: buildings may raise it later.
static constexpr ui8 SETTLEMENT_MAX_RAISED_ARMIES = 1;

// Share of a settlement's population that is raised in armies, in percent rounded down. Raised men count every troop type.
static inline ui32 settlement_raised_percent(ui32 raised_men, ui32 population)
{
	return population > 0 ? (ui32)((ui64)raised_men * 100 / population) : 0;
}

// Population and local troop pool growth run on each settlement once every this many ticks (staggered by the settlement's GUID extra field).
static constexpr ui32 GROWTH_TICK_PERIOD = 50;

// Max local troops of a settlement: a share of its population, rounded down (men at arms 2%, horsemen 1%, knights 0.5%).
// Future cost / upkeep hooks would plug in next to these.
static inline settlement_troops settlement_troop_max(ui32 population)
{
	return {
		(ui16)((ui64)population * 2 / 100),
		(ui16)((ui64)population * 1 / 100),
		(ui16)((ui64)population * 5 / 1000),
	};
}

// Ceiling actually in force: the raw player-set ceiling capped by max.
static inline settlement_troops settlement_troop_effective_ceilings(ui32 population, const settlement_troops& raw_ceilings)
{
	settlement_troops max = settlement_troop_max(population);
	return {
		raw_ceilings.men_at_arms < max.men_at_arms ? raw_ceilings.men_at_arms : max.men_at_arms,
		raw_ceilings.horsemen < max.horsemen ? raw_ceilings.horsemen : max.horsemen,
		raw_ceilings.knights < max.knights ? raw_ceilings.knights : max.knights,
	};
}

#endif // WORLD_SIM_POPULATION_INCLUDED
