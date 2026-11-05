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
    entity_guid origin = caravans.origin_settlements[index];
    bool originValid = caravan_settlement_is_valid(world, origin);
    world_location source = originValid ? settlements.locations[origin.get_index()] : caravans.locations[index];

    entity_guid newDestination;
    ui32 size; // Always the caravan's value: not sized to room.
    // The previous destination is excluded: it was invalid or unreachable, picking it again would loop.
    if (world_caravan_select_destination(world, source, origin, caravans.values[index], 0, false, caravans.flags[index], newDestination, size,
        &caravans.destinations[index], 1))
    {
        caravans.destinations[index] = newDestination;
        world_movement_stop(caravans.movements[index], caravans.locations[index]);
        caravans.movements[index].destination = caravan_destination_tile(world, caravans.flags[index], newDestination);
        return;
    }

    // Nowhere to go: walk back home to refund the value, or give up if there is no home.
    if (!originValid)
    {
        world_destroy_caravan(world, caravan);
        return;
    }

    caravans.flags[index] = (WORLD_ENTITY_CARAVAN_FLAGS)((ui8)caravans.flags[index] | (ui8)WORLD_ENTITY_CARAVAN_FLAGS::RETURNING_HOME);
    caravans.destinations[index] = INVALID_ENTITY_GUID;
    world_movement_stop(caravans.movements[index], caravans.locations[index]);
    caravans.movements[index].destination = caravan_endpoint(world, caravans.flags[index], origin.get_index());
}

// Delivery to an army destination, once the caravan is within 1 tile of it (army in an engagement: within its join radius + 1, and not fighting). The army buys what it has room and gold for (even in an engagement); goods only go to its supplies,
// never to a market. Gold: army -> origin owner's treasury and income (even if same owner). Trade volume: exporter sold + profit (no receiver share), the settlement whose
// territory the army stands in gains sold. Whatever cannot be sold (invalid army, hostile, no room / gold, remainder) is retargeted.
static void caravan_deliver_to_army(match_world_state& world, ui32 caravanIndex)
{
    world_entity_caravans& caravans = world.entities->caravans;
    world_entity_armies& armies = world.entities->armies;
    world_entity_settlements& settlements = world.entities->settlements;
    entity_guid caravan = caravans.guids[caravanIndex];
    entity_guid destination = caravans.destinations[caravanIndex];
    entity_guid origin = caravans.origin_settlements[caravanIndex];

    // Armies as destinations need a valid origin (the selector does the same), its owner decides who is hostile.
    bool valid = caravan_settlement_is_valid(world, origin) && world_entity_is_valid(world, destination);
    if (valid)
    {
        // In an engagement: delivers from its edge (join radius, plus a tile of margin) only while it is not fighting. Otherwise within 1 tile of the army.
        entity_guid engagement = armies.engagements[destination.get_index()];
        if (world_entity_is_valid(world, engagement))
        {
            if (world.entities->engagements.fighting_actives[engagement.get_index()]) return; // Waits at the edge.
            if (!world_location_within_tiles(caravans.locations[caravanIndex], world_entity_get_location(world, engagement), engagement_join_radius(world, engagement) + 1)) return;
        }
        else if (!world_location_within_tiles(caravans.locations[caravanIndex], armies.locations[destination.get_index()], 1)) return; // Still travelling.
    }
    ui16 armyIndex = destination.get_index();
    ui16 originIndex = origin.get_index();
    match_player_id originOwner = settlements.owners[originIndex];
    if (!valid || !army_is_trade_target(world, armyIndex, originOwner))
    {
        world_caravan_retarget(world, caravan);
        return;
    }

    ui32 value = caravans.values[caravanIndex];
    ui32 sold = ia_min(value, ia_min(army_supplies_room(world, armyIndex), army_supplies_buy_power(world, armyIndex)));
    if (sold == 0)
    {
        world_caravan_retarget(world, caravan);
        return;
    }

    // Profit is measured before delivery, as the selector does.
    ui64 profit = caravan_army_trade_profit(world, armyIndex, originOwner, settlement_market_capacity(settlements, originIndex), sold,
        trade_tile_distance(settlements.locations[originIndex], armies.locations[armyIndex]));
    ui16 territory = army_tile_owner(world, armyIndex);

    ui32 cost = sold * ia_max(GAME_PARAMETERS.good_price_gold, (ui32)1); // sold <= gold / price: no underflow.
    armies.supplies[armyIndex] += sold;
    armies.gold[armyIndex] -= cost;
    world_player_state& seller = world.player_states[originOwner];
    seller.treasury += cost;
    seller.income += cost;

    settlement_trade_volume_add(settlements, originIndex, sold + profit);
    if (territory != world_terrain::UNOWNED_TILE) settlement_trade_volume_add(settlements, territory, sold);

    if (sold == value)
    {
        world_destroy_caravan(world, caravan);
        return;
    }
    caravans.values[caravanIndex] = (ui16)(value - sold);
    world_caravan_retarget(world, caravan); // The army is excluded as the previous destination.
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
            entity_guid origin = caravans.origin_settlements[caravanIndex];
            if (caravan_settlement_is_valid(world, origin) && caravans.locations[caravanIndex] == caravan_endpoint(world, caravans.flags[caravanIndex], origin.get_index()))
            {
                settlement_market_add(settlements, origin.get_index(), caravans.values[caravanIndex]); // No profit.
                world_destroy_caravan(world, caravan);
            }
            continue;
        }

        entity_guid destination = caravans.destinations[caravanIndex];
        if (destination.get_type() == ENTITY_TYPE::ARMY)
        {
            caravan_deliver_to_army(world, caravanIndex);
            continue;
        }
        if (!caravan_settlement_is_valid(world, destination)) continue;

        ui16 destinationIndex = destination.get_index();

        // A besieged destination cannot be entered (caravans look for another target before getting here, see world_system_entity_movement_decision).
        if (settlement_is_besieged(world, destinationIndex)) continue;
        if (caravans.locations[caravanIndex] != caravan_destination_tile(world, caravans.flags[caravanIndex], destination)) continue; // Still travelling (ships: until the dock tile).

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

        world_destroy_caravan(world, caravan);
    }
}
