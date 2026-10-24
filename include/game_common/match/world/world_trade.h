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

// Profit of a barter-style exchange: a caravan worth caravan_value offers its goods to a destination, which can only give back what it has.
// destination_pool / destination_capacity = its market goods and capacity, destination_production = its last daily production (settlement_economy::production),
// travel_distance = tile distance between origin and destination (trade_tile_distance; not the path cost).
// The profit is the only value created by a trip: on arrival the origin gets its capital back plus half the profit, the destination the other half.
//   fill = pool / capacity, clamped to [0, 1] (capacity 0: 1 if any goods, else 0).
//   r = lerp(scarcity_value_max_percent, scarcity_value_min_percent, fill): the destination values each caravan good at r% of its own goods (at least 1%).
//   available = pool + production (the whole pool, sellable_percent is not used here).
//   E (caravan goods exchanged) = min(value, available * 100 / r).
//   margin = distance_margin_max_percent% * f, with x = min(d, saturation) - min_tiles (0 at or below distance_margin_min_tiles),
//            f = log2(mid + steepness * x) - log2(mid), divided by the same at x = saturation - min_tiles. f in [0, 1]: rises fast then slowly, flat from the saturation on.
//   profit = E * margin + flat, flat = min(max(investment_min * flat_profit_percent_of_min%, 1), E). E == 0 -> 0.
// Huge inputs: the percents are clamped (10000); intermediates are ui64 and cannot overflow (E < 2^32, percent * progress < 2^24).
static inline ui64 trade_profit(ui32 caravan_value, ui32 destination_pool, ui32 destination_production, ui32 destination_capacity, ui32 travel_distance,
    const game_trade_parameters& trade_parameters)
{
    // Destination fill, fixed point (IA_FIXED_ONE = 1).
    ui64 capacity = destination_capacity;
    ui64 fill = capacity == 0 ? (destination_pool > 0 ? IA_FIXED_ONE : 0) : ia_min((ui64)destination_pool * IA_FIXED_ONE / capacity, (ui64)IA_FIXED_ONE);

    // Value rate and exchanged volume.
    i32 rateMax = (i32)ia_min(trade_parameters.scarcity_value_max_percent, (ui32)10000);
    i32 rateMin = (i32)ia_min(trade_parameters.scarcity_value_min_percent, (ui32)10000);
    ui64 ratePercent = ia_max(ia_lerp_fixed(rateMax, rateMin, (ui32)fill), (i32)1);
    ui64 available = (ui64)destination_pool + destination_production;
    ui64 exchanged = ia_min((ui64)caravan_value, available * 100 / ratePercent);
    if (exchanged == 0) return 0;

    // Distance margin, as a per mille progress along the curve (0 at the minimum distance), then scaled by its max percent.
    ui64 curveProgress = 0;
    ui64 curveMid = trade_parameters.distance_curve_mid;
    ui64 minDistance = trade_parameters.distance_margin_min_tiles;
    ui64 saturation = trade_parameters.distance_saturation;
    ui64 steepness = trade_parameters.distance_curve_steepness;
    if (curveMid > 0 && saturation > minDistance && travel_distance > minDistance)
    {
        ui64 curveDistance = ia_min((ui64)travel_distance, saturation) - minDistance;
        ui32 logCurveMid = ia_log2_q8(curveMid);
        ui64 logAtDistance = ia_log2_q8(curveMid + steepness * curveDistance) - logCurveMid;
        ui64 logAtSaturation = ia_log2_q8(curveMid + steepness * (saturation - minDistance)) - logCurveMid;
        if (logAtSaturation > 0) curveProgress = logAtDistance * 1000 / logAtSaturation; // <= 1000 since the distance is capped.
    }
    ui64 distanceProfit = exchanged * ia_min(trade_parameters.distance_margin_max_percent, (ui32)10000) * curveProgress / (100 * 1000);

    // Flat profit: a constant from the minimum caravan value, at least 1, never more than the exchanged volume.
    ui64 flatProfit = (ui64)trade_parameters.investment_min * ia_min(trade_parameters.flat_profit_percent_of_min, (ui32)10000) / 100;
    flatProfit = ia_min(ia_max(flatProfit, (ui64)1), exchanged);
    return distanceProfit + flatProfit;
}

#endif // WORLD_TRADE_INCLUDED
