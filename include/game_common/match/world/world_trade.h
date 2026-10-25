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


static inline ui64 trade_profit(ui32 caravan_value, ui32 destination_pool, ui32 destination_production, ui32 destination_capacity, ui32 travel_distance,
    const game_trade_parameters& trade_parameters)
{
    // Destination fill, fixed point (IA_FIXED_ONE = 1) + half of the caravan value.
    ui64 destinationExchangeFill = ia_min((ui64)destination_pool * IA_FIXED_ONE / destination_capacity, (ui64)IA_FIXED_ONE) + caravan_value / 2;

    // Value is determined from local scarcity / abundance of goods, and a bonus from distance (to primitively model the extra value of goods coming from faraway).
    i32 scarcityRateMax = trade_parameters.scarcity_value_max_percent;
    i32 scarcityRateMin = trade_parameters.scarcity_value_min_percent;

    ui64 scarcityRatePermille = ia_max(ia_lerp_fixed(scarcityRateMax, scarcityRateMin, destinationExchangeFill), (i32)1) * 10;
    ui32 distanceFactorPermille = travel_distance / trade_parameters.good_val_markup_increase_dist * 1000;

    ui32 exchangeProfit = caravan_value * (scarcityRatePermille + distanceFactorPermille) / 1000;

    // Flat profit: a constant from the minimum caravan value, at least 1, never more than the exchanged volume.
    ui32 flatProfit = trade_parameters.investment_min * trade_parameters.flat_profit_percent_of_min / 100;

    return exchangeProfit + flatProfit;
}

#endif // WORLD_TRADE_INCLUDED
