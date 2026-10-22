// Trade rules shared by the simulation and clients. Pure integer math, no world access.

#ifndef WORLD_TRADE_INCLUDED
#define WORLD_TRADE_INCLUDED

#include "core.h"
#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world.h"

// Distance between two tiles in tiles, integer chamfer approximation of the euclidean distance: max + 2 * min / 5 (weights 5 straight / 7 diagonal, in tiles).
// Exact on straight lines, 1% short on 45 degree lines, up to about 8% over in between.
static inline ui32 trade_tile_distance(world_location from, world_location to)
{
    ui32 dx = from.x > to.x ? from.x - to.x : to.x - from.x;
    ui32 dy = from.y > to.y ? from.y - to.y : to.y - from.y;
    return ia_max(dx, dy) + ia_min(dx, dy) * 2 / 5;
}

// Profit = the extra value ON TOP of the base, of delivering a caravan worth caravan_value to a settlement whose last daily income is destination_income,
// over travel_distance (tile distance between origin and destination, see trade_tile_distance; not the path cost). Revenue (trade_revenue below) = base + profit is what is paid out on arrival.
//   profit = value_bonus + distance_bonus + flat_bonus, with flat_bonus = max(value * profit_flat_percent%, 1): always applies, so profit is never 0 for value > 0.
//   value_bonus   = value * value_bonus_max_percent% * r, with r = income / (income + value) in [0, 1]:
//                    near 1 for a small caravan next to a rich destination, 0.5 when equal, towards 0 as the caravan outgrows it.
//   distance_bonus = value * distance_bonus_max_percent% * f, with f = log2(1 + steepness * d / mid) / log2(1 + steepness * saturation / mid),
//                    d = min(travel_distance, saturation). f is in [0, 1], rises fast then slowly, and is flat from the saturation on.
// Edge cases: value 0 -> 0, any other value -> at least 1. income 0 -> no value bonus. distance 0 -> no distance bonus. curve_mid or saturation 0 -> no distance bonus.
// Huge inputs: the percents are clamped (10000), distance is clamped to the saturation, intermediates are ui64 and cannot overflow.
static inline ui64 trade_profit(ui32 caravan_value, ui32 destination_income, ui32 travel_distance, const game_trade_parameters& trade_parameters)
{
    ui64 value = caravan_value;
    if (value == 0) return 0;

    ui64 profit = 0;

    // Value bonus. income + value > 0 here.
    ui64 income = destination_income;
    ui64 wealthShare = income * 1000 / (income + value); // Per mille.
    profit += value * ia_min(trade_parameters.value_bonus_max_percent, (ui32)10000) * wealthShare / (100 * 1000);

    // Distance bonus.
    ui64 curveMid = trade_parameters.distance_curve_mid;
    ui64 saturation = trade_parameters.distance_saturation;
    ui64 steepness = trade_parameters.distance_curve_steepness;
    if (curveMid > 0 && saturation > 0 && travel_distance > 0)
    {
        ui64 cappedDistance = ia_min((ui64)travel_distance, saturation);
        ui32 logCurveMid = ia_log2_q8(curveMid);
        ui64 logAtDistance = ia_log2_q8(curveMid + steepness * cappedDistance) - logCurveMid;
        ui64 logAtSaturation = ia_log2_q8(curveMid + steepness * saturation) - logCurveMid;
        if (logAtSaturation > 0)
        {
            ui64 curveProgress = logAtDistance * 1000 / logAtSaturation; // Per mille, <= 1000 since the distance is capped.
            profit += value * ia_min(trade_parameters.distance_bonus_max_percent, (ui32)10000) * curveProgress / (100 * 1000);
        }
    }

    // Flat bonus, always added. At least 1 for any caravan with value.
    ui64 flatBonus = value * ia_min(trade_parameters.profit_flat_percent, (ui32)10000) / 100;
    return profit + ia_max(flatBonus, (ui64)1);
}

// Full amount paid out on arrival: value * profit_base_multiplier (clamped to 1000) + trade_profit. Same arguments as trade_profit. 0 for value 0.
// ui64: at most 2^32 * 1000 + profit (profit <= 2^32 * 10000 * 3 / 100), no overflow.
static inline ui64 trade_revenue(ui32 caravan_value, ui32 destination_income, ui32 travel_distance, const game_trade_parameters& trade_parameters)
{
    ui64 base = (ui64)caravan_value * ia_min(trade_parameters.profit_base_multiplier, (ui32)1000);
    return base + trade_profit(caravan_value, destination_income, travel_distance, trade_parameters);
}

#endif // WORLD_TRADE_INCLUDED
