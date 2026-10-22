// Caravan trade system: arrival transactions and the cancel fallback.

#include "game_common/match/world/world_trade.h"
#include "world_sim_entity_caravan.h"

// True if the guid is a settlement that still exists (references can be stale: destroy only invalidates the slot).
static bool caravan_settlement_is_valid(const match_world_state& world, entity_guid settlement)
{
    return settlement.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, settlement);
}

// Adds to a local wealth or trade pool without wrapping around.
static void settlement_wealth_add_clamped(ui32& wealth, ui64 amount)
{
    wealth = (ui32)ia_min((ui64)wealth + amount, (ui64)0xFFFFFFFF);
}

// Adds to a settlement's trade pool up to its cap. Whatever does not fit goes to the settlement's local wealth instead.
static void settlement_trade_pool_add(match_world_state& world, ui16 settlement_index, ui64 amount)
{
    settlement_trading& tradeState = world.entities->settlements.trade[settlement_index];
    ui64 room = tradeState.trade_pool_max > tradeState.trade_pool ? tradeState.trade_pool_max - tradeState.trade_pool : 0;
    ui64 added = ia_min(amount, room);

    tradeState.trade_pool += (ui32)added;
    settlement_wealth_add_clamped(world.entities->settlements.local_wealth[settlement_index], amount - added);
}

void world_caravan_cancel(match_world_state& world, entity_guid caravan)
{
    world_entity_caravans& caravans = world.entities->caravans;
    ui16 index = caravan.get_index();

    // The investment goes back to the origin's trade pool without profit. Without a valid origin it is lost.
    entity_guid origin = caravans.origin_settlements[index];
    if (caravan_settlement_is_valid(world, origin))
    {
        settlement_trade_pool_add(world, origin.get_index(), caravans.values[index]);
    }

    world_destroy_entity_caravan(world, caravan); // Maintains the origin's counter.
}

// Caravans standing on their destination settlement sell their goods and disappear. Caravans whose destination is gone are cancelled.
void world_system_caravan_trade(match_world_state& world)
{
    world_entity_caravans& caravans = world.entities->caravans;
    world_entity_settlements& settlements = world.entities->settlements;

    for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
    {
        entity_guid caravan = caravans.guids[caravanIndex];
        if (!caravan.is_valid()) continue;

        entity_guid destination = caravans.destination_settlements[caravanIndex];
        if (!caravan_settlement_is_valid(world, destination))
        {
            world_caravan_cancel(world, caravan); // Retargeting comes later (task 6).
            continue;
        }

        ui16 destinationIndex = destination.get_index();
        if (caravans.locations[caravanIndex] != settlements.locations[destinationIndex]) continue; // Still travelling.

        entity_guid origin = caravans.origin_settlements[caravanIndex];
        bool originValid = caravan_settlement_is_valid(world, origin);
        ui32 value = caravans.values[caravanIndex];

        // Distance is the tile distance between the settlements. Homeless caravans have no origin to measure from: 0.
        ui32 distance = originValid ? trade_tile_distance(settlements.locations[origin.get_index()], settlements.locations[destinationIndex]) : 0;

        ui64 revenue = trade_revenue(value, settlements.local_wealth[destinationIndex], distance, GAME_PARAMETERS.trade);

        // Split 50/50 (the odd unit goes to the destination), all to the destination if the origin is gone.
        // Placeholder decision: the origin's half refills its trade pool, the destination's half is plain local wealth.
        ui64 originShare = originValid ? revenue / 2 : 0;
        if (originValid) settlement_trade_pool_add(world, origin.get_index(), originShare); // Overflow above the cap goes to its local wealth.
        settlement_wealth_add_clamped(settlements.local_wealth[destinationIndex], revenue - originShare);

        world_destroy_entity_caravan(world, caravan);
    }
}
