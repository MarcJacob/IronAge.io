// Caravan trade system: arrival transactions and the cancel fallback.

#include "game_common/match/world/world_trade.h"
#include "../entities/world_sim_entity_caravan.h"
#include "../entities/world_sim_entity_settlement.h"

// Caravans heading for a besieged settlement look for another destination once this close to it (tiles).
constexpr float CARAVAN_SIEGE_RECHECK_TILES = 30;

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
    ui32 size; // Always the caravan's value: not sized to room.
    // The previous destination is excluded: it was invalid or unreachable, picking it again would loop.
    if (world_caravan_select_destination(world, source, origin, caravans.values[index], 0, false, caravans.flags[index], newDestination, size,
        &caravans.destination_settlements[index], 1))
    {
        caravans.destination_settlements[index] = newDestination;
        world_movement_stop(caravans.movements[index], caravans.locations[index]);
        caravans.movements[index].destination = caravan_endpoint(world, caravans.flags[index], newDestination.get_index());
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
    caravans.movements[index].destination = caravan_endpoint(world, caravans.flags[index], origin.get_index());
}

// Caravans standing on their destination settlement (ships: its dock tile) deliver their goods to it (split by the destination's export ratio), both ends gain trade volume,
// and the caravan disappears. Returning caravans refund their value to their origin on reaching it (split by the origin's export ratio, like any income).
// Invalid destinations are handled by the movement decision.
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
            if (caravan_settlement_is_valid(world, origin) && caravans.locations[caravanIndex] == caravan_endpoint(world, caravans.flags[caravanIndex], origin.get_index()))
            {
                settlement_market_add(settlements, origin.get_index(), caravans.values[caravanIndex]); // No profit.
                world_destroy_entity_caravan(world, caravan);
            }
            continue;
        }

        entity_guid destination = caravans.destination_settlements[caravanIndex];
        if (!caravan_settlement_is_valid(world, destination)) continue;

        ui16 destinationIndex = destination.get_index();

        // A besieged destination cannot be entered: from CARAVAN_SIEGE_RECHECK_TILES away the caravan looks for another target (and keeps its goods).
        if (settlements.sieges[destinationIndex].is_valid())
        {
            if (vec2_dist_squared(caravans.locations[caravanIndex], settlements.locations[destinationIndex]) <= CARAVAN_SIEGE_RECHECK_TILES * CARAVAN_SIEGE_RECHECK_TILES)
                world_caravan_retarget(world, caravan);
            continue;
        }
        if (caravans.locations[caravanIndex] != caravan_endpoint(world, caravans.flags[caravanIndex], destinationIndex)) continue; // Still travelling (ships: until the dock tile).

        entity_guid origin = caravans.origin_settlements[caravanIndex];
        bool originValid = caravan_settlement_is_valid(world, origin);
        ui32 value = caravans.values[caravanIndex];

        // Profit is measured before delivery, from the destination's current state. Homeless caravans have no origin to measure from: no profit.
        ui64 profit = 0;
        if (originValid)
        {
            ui16 originIndex = origin.get_index();
            profit = trade_volume_profit(value, settlement_market_total(settlements, destinationIndex), settlement_market_capacity(settlements, destinationIndex),
                settlement_market_capacity(settlements, originIndex), settlement_trade_volume_fraction(settlements, destinationIndex),
                trade_tile_distance(settlements.locations[originIndex], settlements.locations[destinationIndex]), GAME_PARAMETERS.trade);
        }
        ui64 receiverProfit = trade_profit_receiver_share(profit, GAME_PARAMETERS.trade);

        // The goods are delivered (none are created). Both ends gain volume: value + their share of the profit. Homeless: the exporter side is lost.
        settlement_market_add(settlements, destinationIndex, value);
        settlement_trade_volume_add(settlements, destinationIndex, value + receiverProfit);
        if (originValid) settlement_trade_volume_add(settlements, origin.get_index(), value + profit - receiverProfit);

        world_destroy_entity_caravan(world, caravan);
    }
}
