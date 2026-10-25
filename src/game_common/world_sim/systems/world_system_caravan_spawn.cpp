// Daily caravan spawn system.

#include "game_common/match/world/world_trade.h"
#include "../entities/world_sim_entity_caravan.h"
#include "../entities/world_sim_entity_settlement.h"

// Daily: every settlement rolls once per missing caravan slot. A success invests part of its market goods in a caravan towards a selected destination.
// Spawn chance and investment share grow with the market goods' fill.
void world_system_caravan_spawn(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    const game_trade_parameters& trade = GAME_PARAMETERS.trade;
    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        entity_guid origin = settlements.guids[settlementIndex];
        if (!origin.is_valid()) continue;

        settlement_trading& settlementTrade = settlements.trade[settlementIndex];
        if (settlementTrade.caravan_count >= settlementTrade.caravan_count_max) continue;

        // One independent roll per slot missing at the start of the day.
        ui32 missingSlots = settlementTrade.caravan_count_max - settlementTrade.caravan_count;

        for (ui32 roll = 0; roll < missingSlots; roll++)
        {
            // Re-checked every time: earlier spawns changed the count and the pool.
            if (settlementTrade.caravan_count >= settlementTrade.caravan_count_max) break;
            if (settlementTrade.market_goods < trade.min_pool_to_spawn) break;

            // Chance and investment share go from their min at an empty pool to their max at a full one.
            ui32 fill = settlement_market_fill(settlements, (ui16)settlementIndex);
            ui32 chance = (ui32)ia_lerp_fixed((i32)trade.spawn_chance_min_percent, (i32)trade.spawn_chance_max_percent, fill);
            ui32 investmentPercent = (ui32)ia_lerp_fixed((i32)trade.investment_percent_min, (i32)trade.investment_percent_max, fill);

            // Investment = that share of the pool, at least investment_min (or the whole pool if it holds less). Capped by what a caravan can hold (ui16 value).
            ui64 pool = settlementTrade.market_goods;
            ui64 share = pool * investmentPercent / 100;
            ui64 minimum = ia_min((ui64)trade.investment_min, pool);
            ui32 investment = (ui32)ia_min(ia_min(ia_max(share, minimum), pool), (ui64)0xFFFF); // Never more than the pool, even above 100%.
            if (investment == 0) break;

            if (world.rand_gen.next<ui32>() % 100 >= chance) continue;

            // The pool is only touched once a destination exists.
            // Land caravan (flags 0). The path cost is not kept: the trade system recomputes it on arrival.
            world_entity_caravan caravan = {};
            ui32 pathCost;
            if (!world_caravan_select_destination(world, settlements.locations[settlementIndex], origin, investment, caravan.flags, caravan.destination_settlement, pathCost))
            {
                break; // Nothing changes, and another roll would find nothing either.
            }

            settlementTrade.market_goods -= investment;

            caravan.origin_settlement = origin;
            caravan.value = (ui16)investment;
            caravan.location = settlements.locations[settlementIndex];
            caravan.movement.travel_speed = (ui16)GAME_PARAMETERS.caravan_travel_speed;
            caravan.movement.destination = world_entity_get_location(world, caravan.destination_settlement);
            caravan.movement.move_target = caravan.location;
            caravan.movement.path_handle = INVALID_PATH_HANDLE;

            world_spawn_entity_caravan(world, caravan); // Increments the origin's caravan_count.
        }
    }
}
