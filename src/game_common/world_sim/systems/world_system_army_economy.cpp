// Army economy systems: soldiers consume goods every day.
// Armies carry gold (loot only, never spent) and supplies (goods). Their soldiers consume the supplies daily (per-type upkeep rates).
// Supplies come from caravans, paid for by the army owner's treasury at dispatch (see caravan spawn). Only campaign members are caravan targets. Every army also buys at the settlement it stands on, also with the treasury.
// Consumption that supplies cannot cover makes troops desert (army_low_supply_desertion_percent of the max count, scaled by the share that supplies could not cover), in which case they go back home. A raising army stops raising instead.
// Soldiers waiting in a settlement (local pool) are paid from the treasury instead.

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/entities/world_entity_engagements.h"
#include "game_common/match/world/world.h"
#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_settlement.h"

#include "../world_sim.h"

// Index of the settlement whose territory the army stands in, world_terrain::UNOWNED_TILE if none.
ui16 army_tile_owner(const match_world_state& world, ui16 army_index)
{
	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, world.entities->armies.location[army_index], regionIndex, tileIndex);
	return world.terrain->regions[regionIndex].owners[tileIndex];
}

// Runs daily. Every army consumes. An army whose supplies could not cover it loses men (see the desertion rule below).
void world_system_armies_upkeep(match_world_state& world)
{
	if (!world_is_daily_tick(world)) return;

	world_entity_armies& armies = world.entities->armies;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;

		match_player_id owner = armies.owner[armyIndex];
		if (owner >= world.player_count) continue;

		ui32& supplies = armies.supplies[armyIndex];

		armies.desertions[armyIndex] = 0; // Counts the last day only.

		// Men of an army whose home settlement now belongs to someone else desert: army_wrong_origin_owner_desertion_percent of each type per day, rounded up.
		// They go back to the home settlement. The army is destroyed when its last man leaves: stop at once, its slot is free.
		entity_guid originSettlement = armies.home_settlement[armyIndex];
		// Retinues do not: their men are no part of the origin's population.
		if (!armies.is_retinue[armyIndex] && world_entity_is_valid(world, originSettlement) && originSettlement.get_type() == ENTITY_TYPE::SETTLEMENT
			&& world.entities->settlements.owner[originSettlement.get_index()] != owner)
		{
			entity_guid originArmy = armies.guids[armyIndex];
			army_troop_count troops = armies.composition[armyIndex];
			for (ui8 troopType = 0; troopType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT && world_entity_is_valid(world, originArmy); troopType++)
			{
				ui16 deserters = (ui16)(((ui64)troops.count[troopType] * ia_min(GAME_PARAMETERS.army_wrong_origin_owner_desertion_percent, (ui32)100) + 99) / 100);
				ui16 left = world_army_desertion(world, originArmy, (TROOP_TYPE)troopType, deserters);
				if (world_entity_is_valid(world, originArmy)) armies.desertions[armyIndex] += left; // Destroyed: its stats are gone with it.
			}
			if (!world_entity_is_valid(world, originArmy)) continue;
		}

        // If army is in an engagement where troops are fighting (field battle, assault...), do not run its supply consumption / refill logic.
        entity_guid armyCurrentEngagement = armies.engagement[armyIndex];
        if (armyCurrentEngagement.is_valid() && world.entities->engagements.fighting_active[armyCurrentEngagement.get_index()]) continue;

		ui32 consumption = army_daily_consumption(armies.composition[armyIndex]);
		// Every army buys goods from the market goods of the settlement they stand on (their location equals the settlement's; besieged or not), up to their supplies room,
		// as far as their owner's treasury goes (never their own gold). The payment is a gold sink (nobody is credited), the goods sold only count as trade volume. Same owner only for now.
		ui16 settlementIndex = army_tile_owner(world, (ui16)armyIndex);
		world_entity_settlements& settlements = world.entities->settlements;
		if (settlementIndex != world_terrain::UNOWNED_TILE && settlements.owner[settlementIndex] == owner
			&& armies.location[armyIndex] == settlements.location[settlementIndex])
		{
			ui32 price = ia_max(GAME_PARAMETERS.good_price_gold, (ui32)1);
			world_player_state& player = world.player_states[owner];
			ui32 bought = ia_min(ia_min(army_supplies_room(world, (ui16)armyIndex), player.treasury / price), settlements.trade[settlementIndex].market_goods);
			ui32 cost = bought * price;
			settlements.trade[settlementIndex].market_goods -= bought;
			settlement_trade_volume_add(settlements, settlementIndex, bought);
			player.treasury -= cost; // A pure gold sink: nobody is credited.
			player.expenses += cost;
			supplies += bought;
		}

		// Consumption.
		ui32 consumed = ia_min(consumption, supplies);
		supplies -= consumed;

		// Not enough supplies: a raising army stops raising (keeping its men, no desertion today), any other army loses men.
		ui32 unfunded = consumption - consumed;
		if (unfunded == 0) continue;
		if (armies.raising[armyIndex])
		{
			armies.raising[armyIndex] = 0;
			army_enforce_carry_capacity(world, (ui16)armyIndex);
			continue;
		}

		// Desertion: each troop type loses army_low_supply_desertion_percent x the unfunded share of the consumption (100% when no supply is left) of its max count (what the army was
		// raised to, raised_troops, or its current count if higher), rounded up, never more than the men it has now.
		// The army is destroyed when its last man leaves: stop at once, its slot is free.
		entity_guid army = armies.guids[armyIndex];
		army_troop_count troops = armies.composition[armyIndex];
		army_troop_count raised = armies.raised_troops[armyIndex];
		ui64 percent = ia_min(GAME_PARAMETERS.army_low_supply_desertion_percent, (ui32)100);
		for (ui8 troopType = 0; troopType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; troopType++)
		{
			ui64 maxCount = ia_max(raised.count[troopType], troops.count[troopType]);
			ui64 deserting = (maxCount * percent * unfunded + 100 * (ui64)consumption - 1) / (100 * (ui64)consumption);
			ui16 deserters = (ui16)ia_min(deserting, (ui64)troops.count[troopType]);
			ui16 left = world_army_desertion(world, army, (TROOP_TYPE)troopType, deserters);

			if (!world_entity_is_valid(world, army)) 
                break; // Destroyed: its stats are gone with it.
                       
			armies.desertions[armyIndex] += left;
		}
	}
}
