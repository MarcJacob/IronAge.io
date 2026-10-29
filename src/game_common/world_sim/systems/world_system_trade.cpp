// Caravan / ship trade systems.

#include "game_common/match/world/entities/world_entity_caravans.h"
#include "game_common/match/world/world_pathing.h"
#include "game_common/match/world/world_trade.h"
#include "../entities/world_sim_entity_caravan.h"
#include "../entities/world_sim_entity_settlement.h"
#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_engagement.h"
#include "../world_movement.h"

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
        ui32 pathHandle = world_path_store_get_path(world, startLocation, destinationLocation, 
                caravan_is_ship(caravan.flags) ? PATH_MODE_SEA : PATH_MODE_LAND);
        // Start == destination (an army standing on the origin): the request returns no path, but there is nothing to walk.
        if (startLocation != destinationLocation && pathHandle != INVALID_PATH_HANDLE)
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
        caravan.movement.path_handle = pathHandle;

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

// True if the guid is a settlement that still exists (references can be stale: destroy only invalidates the slot).
static bool caravan_settlement_is_valid(const match_world_state& world, entity_guid settlement)
{
    return settlement.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, settlement);
}

void world_caravan_retarget(match_world_state& world, entity_guid caravan)
{
    world_entity_caravans& caravans = world.entities->caravans;
    world_entity_settlements& settlements = world.entities->settlements;
    ui16 index = caravan.get_index();

    // Select from the origin if it exists (excluding it), otherwise from where the caravan stands. The caravan keeps its value.
    entity_guid origin = caravans.origin_settlement[index];
    bool originValid = caravan_settlement_is_valid(world, origin);
    world_location source = originValid ? settlements.location[origin.get_index()] : caravans.location[index];

    // A caravan paid for by an army's owner loses that army: it goes home, never to another target.
    // Either way the caravan leaves its previous destination: its value stops counting as incoming there (the destination is cleared, so it is released exactly once).
    entity_guid previousDestination = caravans.destination[index];
    bool wasForArmy = previousDestination.get_type() == ENTITY_TYPE::ARMY;
    caravan_release_destination(world, index);

    entity_guid newDestination;
    ui32 size; // Always the caravan's value: not sized to room.
    // The previous destination is excluded: it was invalid or unreachable, picking it again would loop.
    if (!wasForArmy && world_caravan_select_destination(world, source, origin, caravans.value[index], 0, false, caravans.flags[index], newDestination, size,
        &previousDestination, 1))
    {
        caravans.destination[index] = newDestination;
        caravan_add_incoming(world, newDestination, caravans.value[index]);
        world_movement_stop(caravans.movement[index], caravans.location[index]);
        caravans.movement[index].destination = caravan_destination_tile(world, caravans.flags[index], newDestination);
        return;
    }

    // Nowhere to go: walk back home to refund the value, or give up if there is no home.
    if (!originValid)
    {
        world_destroy_caravan(world, caravan);
        return;
    }

    caravans.flags[index] = (WORLD_ENTITY_CARAVAN_FLAGS)((ui8)caravans.flags[index] | (ui8)WORLD_ENTITY_CARAVAN_FLAGS::RETURNING_HOME);
    world_movement_stop(caravans.movement[index], caravans.location[index]);
    caravans.movement[index].destination = caravan_endpoint(world, caravans.flags[index], origin.get_index());
}

// Delivery to an army destination, once the caravan is within 1 tile of it (army in an engagement: within its join radius + 1, and not fighting). The goods were paid at dispatch
// (caravan spawn): the army takes what it has room for (even in an engagement), only into its supplies, never to a market. Trade volume: the settlement whose
// territory the army stands in gains sold. Any delivery destroys the caravan (the remainder is lost). With none (invalid army, hostile, no room) the caravan goes home (world_caravan_retarget).
static void caravan_deliver_to_army(match_world_state& world, ui32 caravanIndex)
{
    world_entity_caravans& caravans = world.entities->caravans;
    world_entity_armies& armies = world.entities->armies;
    world_entity_settlements& settlements = world.entities->settlements;
    entity_guid caravan = caravans.guids[caravanIndex];
    entity_guid destination = caravans.destination[caravanIndex];
    entity_guid origin = caravans.origin_settlement[caravanIndex];

    // Armies as destinations need a valid origin (the selector does the same), its owner decides who is hostile.
    bool valid = caravan_settlement_is_valid(world, origin) && world_entity_is_valid(world, destination);
    if (valid)
    {
        // In an engagement: delivers from its edge (join radius, plus a tile of margin) only while it is not fighting. Otherwise within 1 tile of the army.
        entity_guid engagement = armies.engagement[destination.get_index()];
        if (world_entity_is_valid(world, engagement))
        {
            if (world.entities->engagements.fighting_active[engagement.get_index()]) return; // Waits at the edge.
            if (!world_location_within_tiles(caravans.location[caravanIndex], world_entity_get_location(world, engagement), engagement_join_radius(world, engagement) + 1)) return;
        }
        else if (!world_location_within_tiles(caravans.location[caravanIndex], armies.location[destination.get_index()], 1)) return; // Still travelling.
    }
    ui16 armyIndex = destination.get_index();
    match_player_id originOwner = settlements.owner[origin.get_index()]; // Only valid when the origin is (the check below handles the other case).
    if (!valid || !army_is_trade_target(world, armyIndex, originOwner))
    {
        world_caravan_retarget(world, caravan);
        return;
    }

    ui32 value = caravans.value[caravanIndex];
    // Already paid at dispatch: the army takes what it has room for.
    ui32 sold = ia_min(value, army_supplies_room(world, armyIndex));
    if (sold == 0)
    {
        world_caravan_retarget(world, caravan);
        return;
    }

    ui16 territory = army_tile_owner(world, armyIndex);
    armies.supplies[armyIndex] += sold;
    if (territory != world_terrain::UNOWNED_TILE) settlement_trade_volume_add(settlements, territory, sold);

    // Any delivery ends the caravan (it would hold its origin's caravan slot for too long otherwise): what the army had no room for is lost. Destroying releases its incoming supplies.
    world_destroy_caravan(world, caravan);
}

// Caravans standing on their destination settlement (ships: its dock tile) deliver their goods to it (split by the destination's export ratio), both ends gain trade volume,
// and the caravan disappears. Caravans within 1 tile of their destination army sell it supplies (caravan_deliver_to_army). Returning caravans refund their value to their origin
// on reaching it (split by the origin's export ratio, like any income). Invalid settlement destinations are handled by the movement decision.
void world_system_caravan_trade(match_world_state& world)
{
    world_entity_caravans& caravans = world.entities->caravans;
    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
    {
        entity_guid caravan = caravans.guids[caravanIndex];
        if (!caravan.is_valid()) continue;

        if (((ui8)caravans.flags[caravanIndex] & (ui8)WORLD_ENTITY_CARAVAN_FLAGS::RETURNING_HOME) != 0)
        {
            entity_guid origin = caravans.origin_settlement[caravanIndex];
            if (caravan_settlement_is_valid(world, origin) && caravans.location[caravanIndex] == caravan_endpoint(world, caravans.flags[caravanIndex], origin.get_index()))
            {
                settlement_market_add(settlements, origin.get_index(), caravans.value[caravanIndex]); // No profit.
                world_destroy_caravan(world, caravan);
            }
            continue;
        }

        entity_guid destination = caravans.destination[caravanIndex];
        if (destination.get_type() == ENTITY_TYPE::ARMY)
        {
            caravan_deliver_to_army(world, caravanIndex);
            continue;
        }
        if (!caravan_settlement_is_valid(world, destination)) continue;

        ui16 destinationIndex = destination.get_index();

        // A besieged destination cannot be entered (caravans look for another target before getting here, see world_system_entity_movement_decision).
        if (settlement_is_besieged(world, destinationIndex)) continue;
        if (caravans.location[caravanIndex] != caravan_destination_tile(world, caravans.flags[caravanIndex], destination)) continue; // Still travelling (ships: until the dock tile).

        entity_guid origin = caravans.origin_settlement[caravanIndex];
        bool originValid = caravan_settlement_is_valid(world, origin);
        ui32 value = caravans.value[caravanIndex];

        // The caravan stops being incoming: the profit scarcity below only counts the other caravans on their way.
        caravan_release_destination(world, (ui16)caravanIndex);

        // Profit is measured before delivery, from the destination's current state. Homeless caravans have no origin to measure from: no profit.
        ui64 profit = 0;
        if (originValid)
        {
            ui16 originIndex = origin.get_index();
            profit = trade_volume_profit(value, settlement_market_pool_with_incoming(settlements, destinationIndex), settlement_market_capacity(settlements, destinationIndex),
                settlement_market_capacity(settlements, originIndex), settlement_trade_volume_fraction(settlements, destinationIndex),
                trade_tile_distance(settlements.location[originIndex], settlements.location[destinationIndex]), GAME_PARAMETERS.trade);
        }
        ui64 receiverProfit = trade_profit_receiver_share(profit, GAME_PARAMETERS.trade);

        // The goods are delivered (none are created). Both ends gain volume: value + their share of the profit. Homeless: the exporter side is lost.
        settlement_market_add(settlements, destinationIndex, value);
        settlement_trade_volume_add(settlements, destinationIndex, value + receiverProfit);
        if (originValid) settlement_trade_volume_add(settlements, origin.get_index(), value + profit - receiverProfit);

        world_destroy_caravan(world, caravan);
    }
}
