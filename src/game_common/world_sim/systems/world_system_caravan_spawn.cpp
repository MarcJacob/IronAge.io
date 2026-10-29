// Daily caravan spawn system.

#include "../world_movement.h"
#include "../entities/world_sim_entity_caravan.h"
#include "../entities/world_sim_entity_settlement.h"

static constexpr ui32 CARAVAN_MIN_SIZE_FLOOR = 10; // No caravan carries less, whatever the export pool.
static constexpr ui32 CARAVAN_SPAWN_ATTEMPTS = 3; // Destinations picked per settlement per day (an unreachable one is dropped and another is picked).

// Spawns at most one caravan (or ship) from the settlement, if it has a free slot of that kind. Ships use their own slots, export pool percent, speed,
// water pathing and dock tiles (caravan_endpoint); the trade logic is the same.
static void caravan_try_spawn(match_world_state& world, ui16 settlement_index, bool ship)
{
    world_entity_settlements& settlements = world.entities->settlements;
    entity_guid origin = settlements.guids[settlement_index];
    settlement_trading& settlementTrade = settlements.trade[settlement_index];

    // A besieged settlement trades nothing.
    if (settlements.sieges[settlement_index].is_valid()) return;

    // Checked before anything is drawn: settlements without ship slots (no Trade Port) draw no random number for ships.
    if (ship ? settlementTrade.ship_count >= settlementTrade.ship_count_max : settlementTrade.caravan_count >= settlementTrade.caravan_count_max) return;

    // Caravans and ships share the world's caravan slots: none free, no spawn.
    const world_entity_caravans& caravans = world.entities->caravans;
    if (caravans.active_count >= caravans.max_count) return;

    // Investable: max_caravan_export_pool_percent (ships: max_ship_export_pool_percent) of the export goods (rounded down), never the local market goods.
    // The selector then caps the size per candidate by its room, so this is the size ceiling for min size, profit and score alike.
    ui32 poolPercent = ship ? GAME_PARAMETERS.trade.max_ship_export_pool_percent : GAME_PARAMETERS.trade.max_caravan_export_pool_percent;
    ui64 exportPercent = ia_min(poolPercent, (ui32)100);
    ui32 investable = (ui32)ia_min((ui64)settlementTrade.export_goods * exportPercent / 100, (ui64)0xFFFF); // A caravan holds a ui16 value.
    if (investable < CARAVAN_MIN_SIZE_FLOOR) return;

    world_location originLocation = settlements.locations[settlement_index]; // Selection distances are between settlement tiles, for ships too.

    world_entity_caravan caravan = {}; // Land caravan (flags 0) unless a ship.
    if (ship) caravan.flags = WORLD_ENTITY_CARAVAN_FLAGS::IS_SHIP;
    world_location startLocation = caravan_endpoint(world, caravan.flags, settlement_index);

    // Pick an opportunity, then pathfind only to it. Unreachable ones are excluded from the next picks.
    entity_guid unreachable[CARAVAN_SPAWN_ATTEMPTS];
    for (ui32 attempt = 0; attempt < CARAVAN_SPAWN_ATTEMPTS; attempt++)
    {
        entity_guid destination;
        ui32 size;
        if (!world_caravan_select_destination(world, originLocation, origin, investable, CARAVAN_MIN_SIZE_FLOOR, true, caravan.flags, destination, size, unreachable, attempt))
        {
            return; // No opportunity: no spawn today.
        }

        world_location destinationLocation = caravan_endpoint(world, caravan.flags, destination.get_index());
        path_result path;
        world_request_path(world, startLocation, destinationLocation, caravan_path_params(caravan.flags), path);
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
        caravan.location = startLocation;
        caravan.movement.travel_speed = (ui16)(ship ? GAME_PARAMETERS.ship_travel_speed : GAME_PARAMETERS.caravan_travel_speed);
        caravan.movement.destination = destinationLocation;
        caravan.movement.move_target = caravan.location;
        caravan.movement.path_handle = INVALID_PATH_HANDLE;

        world_spawn_entity_caravan(world, caravan); // Increments the origin's caravan_count (ships: ship_count).
        return;
    }
}

// Daily: every settlement with a free caravan slot looks for a trade opportunity and spawns at most one caravan, sized to it, from its export goods.
// Then the same for ships with a free ship slot (Trade Ports), from the export goods left by the land caravan.
// A destination is eligible if its spawn score reaches min_profit_per_travel_dist_percent (see caravan_spawn_score), picked at random weighted by score.
void world_system_caravan_spawn(match_world_state& world)
{
    if (!world_is_daily_tick(world)) return;

    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;

        caravan_try_spawn(world, (ui16)settlementIndex, false);
        caravan_try_spawn(world, (ui16)settlementIndex, true);
    }
}
