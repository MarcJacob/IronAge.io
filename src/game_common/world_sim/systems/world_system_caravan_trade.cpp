// Caravan trade system: arrival transactions and the cancel fallback.

#include "game_common/match/world/world_trade.h"
#include "../entities/world_sim_entity_caravan.h"
#include "../entities/world_sim_entity_settlement.h"
#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_engagement.h"
#include "../world_movement.h"

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
