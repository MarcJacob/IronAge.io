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


// Trade volume profit of a delivery (volume only, never goods). Split between both ends by trade_profit_receiver_share.
// destination_volume_fraction is the destination's settlement_trade_volume_fraction (fixed point). A 0 capacity counts as a full pool.
static inline ui64 trade_volume_profit(ui32 caravan_value, ui32 destination_pool, ui32 destination_capacity, ui32 origin_capacity,
    ui32 destination_volume_fraction, ui32 tile_distance, const game_trade_parameters& trade_parameters)
{
    // Destination fill once half the caravan is delivered, fixed point, clamped to full.
    ui64 fill = IA_FIXED_ONE;
    if (destination_capacity > 0)
        fill = ia_min(((ui64)destination_pool + caravan_value / 2) * IA_FIXED_ONE / destination_capacity, (ui64)IA_FIXED_ONE);

    // Scarcity rate of the destination plus a distance bonus (value x distance / good_val_markup_increase_dist), per mille.
    i32 scarcityMax = (i32)ia_min(trade_parameters.scarcity_value_max_percent, (ui32)10000);
    i32 scarcityMin = (i32)ia_min(trade_parameters.scarcity_value_min_percent, (ui32)10000);
    ui64 scarcityPermille = (ui64)ia_max(ia_lerp_fixed(scarcityMax, scarcityMin, (ui32)fill), (i32)1) * 10;
    ui64 distancePermille = (ui64)tile_distance * 1000 / ia_max(trade_parameters.good_val_markup_increase_dist, (ui16)1);
    ui64 profit = (ui64)caravan_value * (scarcityPermille + distancePermille) / 1000;

    // Capacity scaling: (destination - origin) x 100 / origin, clamped to [-malus, +bonus]. Origin 0: full bonus if the destination has any capacity.
    i64 bonusMax = (i64)ia_min(trade_parameters.capacity_ratio_max_bonus_percent, (ui32)10000);
    i64 malusMax = (i64)ia_min(trade_parameters.capacity_ratio_max_malus_percent, (ui32)100);
    i64 capacityPercent = 0;
    if (origin_capacity > 0) capacityPercent = ((i64)destination_capacity - (i64)origin_capacity) * 100 / (i64)origin_capacity;
    else if (destination_capacity > 0) capacityPercent = bonusMax;
    capacityPercent = ia_min(ia_max(capacityPercent, -malusMax), bonusMax);
    profit = profit * (ui64)(100 + capacityPercent) / 100;

    // Destination trade bonus: up to incoming_profit_bonus_percent_max more, linear in its volume fraction.
    ui64 incomingPercentMax = ia_min(trade_parameters.incoming_profit_bonus_percent_max, (ui32)10000);
    ui64 fraction = ia_min(destination_volume_fraction, IA_FIXED_ONE);
    profit = profit * (100ull * IA_FIXED_ONE + incomingPercentMax * fraction) / (100ull * IA_FIXED_ONE);

    // Short-trade penalty on the whole profit: 0 up to distance_margin_min_tiles, linear to 100% at twice that, 100% beyond. 0 = no ramp.
    ui64 minTiles = trade_parameters.distance_margin_min_tiles;
    if (minTiles > 0)
    {
        ui64 ramp = tile_distance > minTiles ? ia_min((ui64)tile_distance - minTiles, minTiles) : 0;
        profit = profit * ramp / minTiles;
    }

    return profit;
}

// Receiver's part of a profit (profit_receiver_percent, clamped to 100). The exporter's part is the rest.
static inline ui64 trade_profit_receiver_share(ui64 profit, const game_trade_parameters& trade_parameters)
{
    return profit * ia_min(trade_parameters.profit_receiver_percent, (ui32)100) / 100;
}

#endif // WORLD_TRADE_INCLUDED
