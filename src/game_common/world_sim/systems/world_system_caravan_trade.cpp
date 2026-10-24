// Caravan trade system: arrival transactions and the cancel fallback.

#include "game_common/match/world/world_trade.h"
#include "../entities/world_sim_entity_caravan.h"
#include "../entities/world_sim_entity_settlement.h"

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
    entity_guid origin = caravans.origin_settlements[index];
    bool originValid = caravan_settlement_is_valid(world, origin);
    world_location source = originValid ? settlements.locations[origin.get_index()] : caravans.locations[index];

    entity_guid newDestination;
    ui32 pathCost; // Unused: the trade system measures the tile distance on arrival.
    if (world_caravan_select_destination(world, source, origin, caravans.values[index], caravans.flags[index], newDestination, pathCost))
    {
        caravans.destination_settlements[index] = newDestination;
        world_movement_stop(caravans.movements[index], caravans.locations[index]);
        caravans.movements[index].destination = settlements.locations[newDestination.get_index()];
        return;
    }

    // Nowhere to go: walk back home to refund the value, or give up if there is no home.
    if (!originValid)
    {
        world_destroy_entity_caravan(world, caravan);
        return;
    }

    caravans.flags[index] = (WORLD_ENTITY_CARAVAN_FLAGS)((ui8)caravans.flags[index] | (ui8)WORLD_ENTITY_CARAVAN_FLAGS::RETURNING_HOME);
    caravans.destination_settlements[index] = INVALID_ENTITY_GUID;
    world_movement_stop(caravans.movements[index], caravans.locations[index]);
    caravans.movements[index].destination = settlements.locations[origin.get_index()];
}

// Caravans standing on their destination settlement sell their goods and disappear.
// Returning caravans refund their value to their origin's market goods on reaching it. Invalid destinations are handled by the movement decision.
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
            entity_guid origin = caravans.origin_settlements[caravanIndex];
            if (caravan_settlement_is_valid(world, origin) && caravans.locations[caravanIndex] == settlements.locations[origin.get_index()])
            {
                settlement_market_add(settlements, origin.get_index(), caravans.values[caravanIndex]); // No profit.
                world_destroy_entity_caravan(world, caravan);
            }
            continue;
        }

        entity_guid destination = caravans.destination_settlements[caravanIndex];
        if (!caravan_settlement_is_valid(world, destination)) continue;

        ui16 destinationIndex = destination.get_index();
        if (caravans.locations[caravanIndex] != settlements.locations[destinationIndex]) continue; // Still travelling.

        entity_guid origin = caravans.origin_settlements[caravanIndex];
        bool originValid = caravan_settlement_is_valid(world, origin);
        ui32 value = caravans.values[caravanIndex];

        // Distance is the tile distance between the settlements. Homeless caravans have no origin to measure from: 0.
        ui32 distance = originValid ? trade_tile_distance(settlements.locations[origin.get_index()], settlements.locations[destinationIndex]) : 0;

        ui64 profit = trade_profit(value, settlements.trade[destinationIndex].market_goods, settlements.economies[destinationIndex].production, settlement_market_capacity(settlements, destinationIndex), distance, GAME_PARAMETERS.trade);

        // Origin: capital back + half the profit. Destination: the other half. Homeless: the destination gets the profit, the capital is lost.
        ui64 originProfit = originValid ? profit / 2 : 0;
        if (originValid) settlement_market_add(settlements, origin.get_index(), value + originProfit);
        settlement_market_add(settlements, destinationIndex, profit - originProfit);

        world_destroy_entity_caravan(world, caravan);
    }
}
