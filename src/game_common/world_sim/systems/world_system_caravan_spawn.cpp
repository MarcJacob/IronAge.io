// Daily caravan spawn system.

#include "../world_movement.h"
#include "../entities/world_sim_entity_caravan.h"
#include "../entities/world_sim_entity_settlement.h"

static constexpr ui32 CARAVAN_MIN_SIZE_FLOOR = 10; // No caravan carries less, whatever the pool.
static constexpr ui32 CARAVAN_SPAWN_ATTEMPTS = 3; // Destinations picked per settlement per day (an unreachable one is dropped and another is picked).

// Minimum worthwhile caravan size: a % of the pool, linear in free slots from min_size_percent_last_free (1 free) to min_size_percent_all_free (all free).
// A settlement with a single slot uses the last-free value. Percents clamped to 100. At least CARAVAN_MIN_SIZE_FLOOR.
static ui32 caravan_min_size(ui32 pool, ui32 free_slots, ui32 max_slots)
{
    const game_trade_parameters& trade = GAME_PARAMETERS.trade;
    i64 allFree = ia_min(trade.min_size_percent_all_free, (ui32)100);
    i64 lastFree = ia_min(trade.min_size_percent_last_free, (ui32)100);

    i64 percent = lastFree;
    if (max_slots > 1) percent = lastFree + (allFree - lastFree) * (i64)(free_slots - 1) / (i64)(max_slots - 1);

    return ia_max((ui32)((ui64)pool * (ui64)percent / 100), CARAVAN_MIN_SIZE_FLOOR);
}

// Daily: every settlement with a free caravan slot looks for a trade opportunity and spawns at most one caravan, sized to it.
// A destination is eligible if its spawn score reaches min_profit_per_travel_dist_percent (see caravan_spawn_score), picked at random weighted by score.
void world_system_caravan_spawn(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        entity_guid origin = settlements.guids[settlementIndex];
        if (!origin.is_valid()) continue;

        settlement_trading& settlementTrade = settlements.trade[settlementIndex];
        if (settlementTrade.caravan_count >= settlementTrade.caravan_count_max) continue;

        // Investable: at most the pool (starving the origin for a very good deal is intended), and at most max_caravan_capacity_percent of the origin's capacity.
        // The selector then caps the size per candidate by its room, so this is the size ceiling for min size, profit and score alike.
        ui64 pool = settlementTrade.market_goods;
        ui64 sizeCap = (ui64)settlement_market_capacity(settlements, (ui16)settlementIndex) * GAME_PARAMETERS.trade.max_caravan_capacity_percent / 100;
        ui32 investable = (ui32)ia_min(ia_min(pool, sizeCap), (ui64)0xFFFF); // A caravan holds a ui16 value.

        ui32 minSize = caravan_min_size((ui32)pool, settlementTrade.caravan_count_max - settlementTrade.caravan_count, settlementTrade.caravan_count_max);
        if (investable < minSize) continue;

        world_location originLocation = settlements.locations[settlementIndex];

        // Pick an opportunity, then pathfind only to it. Unreachable ones are excluded from the next picks.
        world_entity_caravan caravan = {}; // Land caravan (flags 0).
        entity_guid unreachable[CARAVAN_SPAWN_ATTEMPTS];
        for (ui32 attempt = 0; attempt < CARAVAN_SPAWN_ATTEMPTS; attempt++)
        {
            entity_guid destination;
            ui32 size;
            if (!world_caravan_select_destination(world, originLocation, origin, investable, minSize, true, caravan.flags, destination, size, unreachable, attempt))
            {
                break; // No opportunity: no spawn today.
            }

            path_result path;
            world_request_path(world, originLocation, settlements.locations[destination.get_index()], CARAVAN_PATH_PARAMS, path);
            if (path.count == 0 || path.partial)
            {
                unreachable[attempt] = destination;
                continue;
            }

            // The investment leaves the pool only now.
            settlementTrade.market_goods -= size;

            caravan.origin_settlement = origin;
            caravan.destination_settlement = destination;
            caravan.value = (ui16)size;
            caravan.location = originLocation;
            caravan.movement.travel_speed = (ui16)GAME_PARAMETERS.caravan_travel_speed;
            caravan.movement.destination = settlements.locations[destination.get_index()];
            caravan.movement.move_target = caravan.location;
            caravan.movement.path_handle = INVALID_PATH_HANDLE;

            world_spawn_entity_caravan(world, caravan); // Increments the origin's caravan_count.
            break;
        }
    }
}
