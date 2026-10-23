// Daily caravan spawn system.

#include "game_common/match/world/world_trade.h"
#include "../entities/world_sim_entity_caravan.h"

// Spawn probability (%) for a settlement with the given number of missing caravan slots (>= 1): linear from min at 1 to max at max_missing_slots or more.
static ui32 caravan_spawn_chance_percent(ui32 missing_slots)
{
    const game_trade_parameters& trade = GAME_PARAMETERS.trade;
    if (missing_slots >= trade.spawn_chance_max_missing_slots) return trade.spawn_chance_max_percent;

    // Here 1 <= missing_slots < max_missing_slots, so max_missing_slots >= 2.
    i32 range = (i32)trade.spawn_chance_max_percent - (i32)trade.spawn_chance_min_percent;
    return (ui32)((i32)trade.spawn_chance_min_percent + range * (i32)(missing_slots - 1) / (i32)(trade.spawn_chance_max_missing_slots - 1));
}

// Daily: every settlement rolls once per missing caravan slot. A success invests part of its export pool in a caravan towards a selected destination.
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
        ui32 chance = caravan_spawn_chance_percent(missingSlots);

        for (ui32 roll = 0; roll < missingSlots; roll++)
        {
            // Re-checked every time: earlier spawns changed the count and the pool.
            if (settlementTrade.caravan_count >= settlementTrade.caravan_count_max) break;
            if (settlementTrade.export_pool < trade.min_export_pool_to_spawn) break;

            // Investment = a share of the pool, at least investment_min (or the whole pool if it holds less). Capped by what a caravan can hold (ui16 value).
            ui64 pool = settlementTrade.export_pool;
            ui64 share = pool * trade.investment_percent / 100;
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

            settlementTrade.export_pool -= investment;

            caravan.origin_settlement = origin;
            caravan.value = (ui16)investment;
            caravan.location = settlements.locations[settlementIndex];
            caravan.movement.travel_speed = 4; // Same as workers and armies.
            caravan.movement.destination = world_entity_get_location(world, caravan.destination_settlement);
            caravan.movement.move_target = caravan.location;
            caravan.movement.path_handle = INVALID_PATH_HANDLE;

            world_spawn_entity_caravan(world, caravan); // Increments the origin's caravan_count.
        }
    }
}
