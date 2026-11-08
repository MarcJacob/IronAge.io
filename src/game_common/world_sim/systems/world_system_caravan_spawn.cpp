// Periodic caravan spawn system.

#include "game_common/match/world/world_trade.h"
#include "../world_movement.h"
#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_caravan.h"
#include "../entities/world_sim_entity_settlement.h"

static constexpr ui32 CARAVAN_MIN_SIZE_FLOOR = 10; // No caravan carries less, whatever the export pool.
static constexpr ui32 CARAVAN_SPAWN_ATTEMPTS = 3; // Destinations picked per settlement per check (an unreachable one is dropped and another is picked).

// Spawns at most one caravan (or ship) from the settlement, if it has a free slot of that kind. Ships use their own slots, export pool percent, speed,
// water pathing and dock tiles (caravan_endpoint); the trade logic is the same.
static void caravan_try_spawn(match_world_state& world, ui16 settlement_index, bool ship)
{
    world_entity_settlements& settlements = world.entities->settlements;
    entity_guid origin = settlements.guids[settlement_index];
    settlement_trading& settlementTrade = settlements.trade[settlement_index];

    // A besieged settlement trades nothing.
    if (settlement_is_besieged(world, settlement_index)) return;

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

    world_location originLocation = settlements.location[settlement_index]; // Selection distances are between settlement tiles, for ships too.

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

        world_location destinationLocation = caravan_destination_tile(world, caravan.flags, destination);
        path_result path;
        world_request_path(world, startLocation, destinationLocation, caravan_path_params(caravan.flags), path);
        // Start == destination (an army standing on the origin): the request returns no path, but there is nothing to walk.
        if (startLocation != destinationLocation && (path.count == 0 || path.partial))
        {
            unreachable[attempt] = destination;
            continue;
        }

        // The investment leaves the export pool only now (size <= investable <= export goods).
        settlementTrade.export_goods -= size;

        // Supplies for an army are paid now by the army owner's treasury (size <= its buy power): a pure gold sink, nobody is credited. The settlement only gains
        // trade volume (size + profit). The army's own gold is never touched.
        if (destination.get_type() == ENTITY_TYPE::ARMY)
        {
            ui16 armyIndex = destination.get_index();
            match_player_id originOwner = settlements.owner[settlement_index];
            ui32 cost = size * ia_max(GAME_PARAMETERS.good_price_gold, (ui32)1);
            world_player_state& payer = world.player_states[world.entities->armies.owner[armyIndex]];
            payer.treasury -= cost;
            payer.expenses += cost;
            ui64 profit = caravan_army_trade_profit(world, armyIndex, originOwner, settlement_market_capacity(settlements, settlement_index), size,
                trade_tile_distance(originLocation, world.entities->armies.location[armyIndex]));
            settlement_trade_volume_add(settlements, settlement_index, size + profit);
        }

        caravan.origin_settlement = origin;
        caravan.destination = destination;
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

// Every trade.caravan_spawn_period_ticks ticks of each settlement's age (staggered by its founding tick): if it has a free caravan slot it looks for a trade
// opportunity and spawns at most one caravan, sized to it, from its export goods.
// Then the same for ships with a free ship slot (Trade Ports), from the export goods left by the land caravan.
// A destination is eligible if its spawn score reaches min_profit_per_travel_dist_percent (see caravan_spawn_score), picked at random weighted by score.
void world_system_caravan_spawn(match_world_state& world)
{
    world_entity_settlements& settlements = world.entities->settlements;
    ui64 period = ia_max(GAME_PARAMETERS.trade.caravan_spawn_period_ticks, (ui32)1);

    for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
    {
        if (!settlements.guids[settlementIndex].is_valid()) continue;
        if (world.age < settlements.founding_tick[settlementIndex] || (world.age - settlements.founding_tick[settlementIndex]) % period != 0) continue;

        caravan_try_spawn(world, (ui16)settlementIndex, false);
        caravan_try_spawn(world, (ui16)settlementIndex, true);
    }
}
