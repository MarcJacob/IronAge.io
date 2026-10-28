// Daily caravan spawn system.

#include "../world_movement.h"
#include "../entities/world_sim_entity_caravan.h"
#include "../entities/world_sim_entity_settlement.h"

static constexpr ui32 CARAVAN_MIN_SIZE_FLOOR = 10; // No caravan carries less, whatever the export pool.
static constexpr ui32 CARAVAN_SPAWN_ATTEMPTS = 3; // Destinations picked per settlement per day (an unreachable one is dropped and another is picked).

// Daily: every settlement with a free caravan slot looks for a trade opportunity and spawns at most one caravan, sized to it, from its export goods.
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

        // Investable: max_caravan_export_pool_percent of the export goods (rounded down), never the local market goods.
        // The selector then caps the size per candidate by its room, so this is the size ceiling for min size, profit and score alike.
        ui64 exportPercent = ia_min(GAME_PARAMETERS.trade.max_caravan_export_pool_percent, (ui32)100);
        ui32 investable = (ui32)ia_min((ui64)settlementTrade.export_goods * exportPercent / 100, (ui64)0xFFFF); // A caravan holds a ui16 value.
        if (investable < CARAVAN_MIN_SIZE_FLOOR) continue;

        world_location originLocation = settlements.locations[settlementIndex];

        // Pick an opportunity, then pathfind only to it. Unreachable ones are excluded from the next picks.
        world_entity_caravan caravan = {}; // Land caravan (flags 0).
        entity_guid unreachable[CARAVAN_SPAWN_ATTEMPTS];
        for (ui32 attempt = 0; attempt < CARAVAN_SPAWN_ATTEMPTS; attempt++)
        {
            entity_guid destination;
            ui32 size;
            if (!world_caravan_select_destination(world, originLocation, origin, investable, CARAVAN_MIN_SIZE_FLOOR, true, caravan.flags, destination, size, unreachable, attempt))
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

            // The investment leaves the export pool only now (size <= investable <= export goods).
            settlementTrade.export_goods -= size;

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
